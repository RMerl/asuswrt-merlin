/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file or.h
 * \brief Master header file for Tor-specific functionality.
 **/

#ifndef TOR_OR_H
#define TOR_OR_H

#include "orconfig.h"

#ifdef __COVERITY__
/* If we're building for a static analysis, turn on all the off-by-default
 * features. */
#ifndef INSTRUMENT_DOWNLOADS
#define INSTRUMENT_DOWNLOADS 1
#endif
#endif

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h> /* FreeBSD needs this to know what version it is */
#endif
#include "torint.h"
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <direct.h>
#include <windows.h>
#endif

#ifdef USE_BUFFEREVENTS
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>
#endif

#include "crypto.h"
#include "tortls.h"
#include "../common/torlog.h"
#include "container.h"
#include "torgzip.h"
#include "address.h"
#include "compat_libevent.h"
#include "ht.h"
#include "replaycache.h"
#include "crypto_curve25519.h"
#include "tor_queue.h"

/* These signals are defined to help handle_control_signal work.
 */
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGINT
#define SIGINT 2
#endif
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGUSR2
#define SIGUSR2 12
#endif
#ifndef SIGTERM
#define SIGTERM 15
#endif
/* Controller signals start at a high number so we don't
 * conflict with system-defined signals. */
#define SIGNEWNYM 129
#define SIGCLEARDNSCACHE 130

#if (SIZEOF_CELL_T != 0)
/* On Irix, stdlib.h defines a cell_t type, so we need to make sure
 * that our stuff always calls cell_t something different. */
#define cell_t tor_cell_t
#endif

#ifdef ENABLE_TOR2WEB_MODE
#define NON_ANONYMOUS_MODE_ENABLED 1
#endif

/** Length of longest allowable configured nickname. */
#define MAX_NICKNAME_LEN 19
/** Length of a router identity encoded as a hexadecimal digest, plus
 * possible dollar sign. */
#define MAX_HEX_NICKNAME_LEN (HEX_DIGEST_LEN+1)
/** Maximum length of verbose router identifier: dollar sign, hex ID digest,
 * equal sign or tilde, nickname. */
#define MAX_VERBOSE_NICKNAME_LEN (1+HEX_DIGEST_LEN+1+MAX_NICKNAME_LEN)

/** Maximum size, in bytes, for resized buffers. */
#define MAX_BUF_SIZE ((1<<24)-1) /* 16MB-1 */
/** Maximum size, in bytes, for any directory object that we've downloaded. */
#define MAX_DIR_DL_SIZE MAX_BUF_SIZE

/** For HTTP parsing: Maximum number of bytes we'll accept in the headers
 * of an HTTP request or response. */
#define MAX_HEADERS_SIZE 50000
/** Maximum size, in bytes, for any directory object that we're accepting
 * as an upload. */
#define MAX_DIR_UL_SIZE MAX_BUF_SIZE

/** Maximum size, in bytes, of a single router descriptor uploaded to us
 * as a directory authority. Caches and clients fetch whatever descriptors
 * the authorities tell them to fetch, and don't care about size. */
#define MAX_DESCRIPTOR_UPLOAD_SIZE 20000

/** Maximum size of a single extrainfo document, as above. */
#define MAX_EXTRAINFO_UPLOAD_SIZE 50000

/** How long do we keep DNS cache entries before purging them (regardless of
 * their TTL)? */
#define MAX_DNS_ENTRY_AGE (30*60)
/** How long do we cache/tell clients to cache DNS records when no TTL is
 * known? */
#define DEFAULT_DNS_TTL (30*60)
/** How long can a TTL be before we stop believing it? */
#define MAX_DNS_TTL (3*60*60)
/** How small can a TTL be before we stop believing it?  Provides rudimentary
 * pinning. */
#define MIN_DNS_TTL 60

/** How often do we rotate onion keys? */
#define MIN_ONION_KEY_LIFETIME (7*24*60*60)
/** How often do we rotate TLS contexts? */
#define MAX_SSL_KEY_LIFETIME_INTERNAL (2*60*60)

/** How old do we allow a router to get before removing it
 * from the router list? In seconds. */
#define ROUTER_MAX_AGE (60*60*48)
/** How old can a router get before we (as a server) will no longer
 * consider it live? In seconds. */
#define ROUTER_MAX_AGE_TO_PUBLISH (60*60*24)
/** How old do we let a saved descriptor get before force-removing it? */
#define OLD_ROUTER_DESC_MAX_AGE (60*60*24*5)

/** Possible rules for generating circuit IDs on an OR connection. */
typedef enum {
  CIRC_ID_TYPE_LOWER=0, /**< Pick from 0..1<<15-1. */
  CIRC_ID_TYPE_HIGHER=1, /**< Pick from 1<<15..1<<16-1. */
  /** The other side of a connection is an OP: never create circuits to it,
   * and let it use any circuit ID it wants. */
  CIRC_ID_TYPE_NEITHER=2
} circ_id_type_t;
#define circ_id_type_bitfield_t ENUM_BF(circ_id_type_t)

#define CONN_TYPE_MIN_ 3
/** Type for sockets listening for OR connections. */
#define CONN_TYPE_OR_LISTENER 3
/** A bidirectional TLS connection transmitting a sequence of cells.
 * May be from an OR to an OR, or from an OP to an OR. */
#define CONN_TYPE_OR 4
/** A TCP connection from an onion router to a stream's destination. */
#define CONN_TYPE_EXIT 5
/** Type for sockets listening for SOCKS connections. */
#define CONN_TYPE_AP_LISTENER 6
/** A SOCKS proxy connection from the user application to the onion
 * proxy. */
#define CONN_TYPE_AP 7
/** Type for sockets listening for HTTP connections to the directory server. */
#define CONN_TYPE_DIR_LISTENER 8
/** Type for HTTP connections to the directory server. */
#define CONN_TYPE_DIR 9
/** Connection from the main process to a CPU worker process. */
#define CONN_TYPE_CPUWORKER 10
/** Type for listening for connections from user interface process. */
#define CONN_TYPE_CONTROL_LISTENER 11
/** Type for connections from user interface process. */
#define CONN_TYPE_CONTROL 12
/** Type for sockets listening for transparent connections redirected by pf or
 * netfilter. */
#define CONN_TYPE_AP_TRANS_LISTENER 13
/** Type for sockets listening for transparent connections redirected by
 * natd. */
#define CONN_TYPE_AP_NATD_LISTENER 14
/** Type for sockets listening for DNS requests. */
#define CONN_TYPE_AP_DNS_LISTENER 15

/** Type for connections from the Extended ORPort. */
#define CONN_TYPE_EXT_OR 16
/** Type for sockets listening for Extended ORPort connections. */
#define CONN_TYPE_EXT_OR_LISTENER 17

#define CONN_TYPE_MAX_ 17
/* !!!! If _CONN_TYPE_MAX is ever over 31, we must grow the type field in
 * connection_t. */

/* Proxy client types */
#define PROXY_NONE 0
#define PROXY_CONNECT 1
#define PROXY_SOCKS4 2
#define PROXY_SOCKS5 3
/* !!!! If there is ever a PROXY_* type over 2, we must grow the proxy_type
 * field in or_connection_t */

/* Pluggable transport proxy type. Don't use this in or_connection_t,
 * instead use the actual underlying proxy type (see above).  */
#define PROXY_PLUGGABLE 4

/* Proxy client handshake states */
/* We use a proxy but we haven't even connected to it yet. */
#define PROXY_INFANT 1
/* We use an HTTP proxy and we've sent the CONNECT command. */
#define PROXY_HTTPS_WANT_CONNECT_OK 2
/* We use a SOCKS4 proxy and we've sent the CONNECT command. */
#define PROXY_SOCKS4_WANT_CONNECT_OK 3
/* We use a SOCKS5 proxy and we try to negotiate without
   any authentication . */
#define PROXY_SOCKS5_WANT_AUTH_METHOD_NONE 4
/* We use a SOCKS5 proxy and we try to negotiate with
   Username/Password authentication . */
#define PROXY_SOCKS5_WANT_AUTH_METHOD_RFC1929 5
/* We use a SOCKS5 proxy and we just sent our credentials. */
#define PROXY_SOCKS5_WANT_AUTH_RFC1929_OK 6
/* We use a SOCKS5 proxy and we just sent our CONNECT command. */
#define PROXY_SOCKS5_WANT_CONNECT_OK 7
/* We use a proxy and we CONNECTed successfully!. */
#define PROXY_CONNECTED 8

/** True iff <b>x</b> is an edge connection. */
#define CONN_IS_EDGE(x) \
  ((x)->type == CONN_TYPE_EXIT || (x)->type == CONN_TYPE_AP)

/** State for any listener connection. */
#define LISTENER_STATE_READY 0

#define CPUWORKER_STATE_MIN_ 1
/** State for a connection to a cpuworker process that's idle. */
#define CPUWORKER_STATE_IDLE 1
/** State for a connection to a cpuworker process that's processing a
 * handshake. */
#define CPUWORKER_STATE_BUSY_ONION 2
#define CPUWORKER_STATE_MAX_ 2

#define CPUWORKER_TASK_ONION CPUWORKER_STATE_BUSY_ONION
#define CPUWORKER_TASK_SHUTDOWN 255

#define OR_CONN_STATE_MIN_ 1
/** State for a connection to an OR: waiting for connect() to finish. */
#define OR_CONN_STATE_CONNECTING 1
/** State for a connection to an OR: waiting for proxy handshake to complete */
#define OR_CONN_STATE_PROXY_HANDSHAKING 2
/** State for an OR connection client: SSL is handshaking, not done
 * yet. */
#define OR_CONN_STATE_TLS_HANDSHAKING 3
/** State for a connection to an OR: We're doing a second SSL handshake for
 * renegotiation purposes. (V2 handshake only.) */
#define OR_CONN_STATE_TLS_CLIENT_RENEGOTIATING 4
/** State for a connection at an OR: We're waiting for the client to
 * renegotiate (to indicate a v2 handshake) or send a versions cell (to
 * indicate a v3 handshake) */
#define OR_CONN_STATE_TLS_SERVER_RENEGOTIATING 5
/** State for an OR connection: We're done with our SSL handshake, we've done
 * renegotiation, but we haven't yet negotiated link protocol versions and
 * sent a netinfo cell. */
#define OR_CONN_STATE_OR_HANDSHAKING_V2 6
/** State for an OR connection: We're done with our SSL handshake, but we
 * haven't yet negotiated link protocol versions, done a V3 handshake, and
 * sent a netinfo cell. */
#define OR_CONN_STATE_OR_HANDSHAKING_V3 7
/** State for an OR connection: Ready to send/receive cells. */
#define OR_CONN_STATE_OPEN 8
#define OR_CONN_STATE_MAX_ 8

/** States of the Extended ORPort protocol. Be careful before changing
 *  the numbers: they matter. */
#define EXT_OR_CONN_STATE_MIN_ 1
/** Extended ORPort authentication is waiting for the authentication
 *  type selected by the client. */
#define EXT_OR_CONN_STATE_AUTH_WAIT_AUTH_TYPE 1
/** Extended ORPort authentication is waiting for the client nonce. */
#define EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_NONCE 2
/** Extended ORPort authentication is waiting for the client hash. */
#define EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_HASH 3
#define EXT_OR_CONN_STATE_AUTH_MAX 3
/** Authentication finished and the Extended ORPort is now accepting
 *  traffic. */
#define EXT_OR_CONN_STATE_OPEN 4
/** Extended ORPort is flushing its last messages and preparing to
 *  start accepting OR connections. */
#define EXT_OR_CONN_STATE_FLUSHING 5
#define EXT_OR_CONN_STATE_MAX_ 5

#define EXIT_CONN_STATE_MIN_ 1
/** State for an exit connection: waiting for response from DNS farm. */
#define EXIT_CONN_STATE_RESOLVING 1
/** State for an exit connection: waiting for connect() to finish. */
#define EXIT_CONN_STATE_CONNECTING 2
/** State for an exit connection: open and ready to transmit data. */
#define EXIT_CONN_STATE_OPEN 3
/** State for an exit connection: waiting to be removed. */
#define EXIT_CONN_STATE_RESOLVEFAILED 4
#define EXIT_CONN_STATE_MAX_ 4

/* The AP state values must be disjoint from the EXIT state values. */
#define AP_CONN_STATE_MIN_ 5
/** State for a SOCKS connection: waiting for SOCKS request. */
#define AP_CONN_STATE_SOCKS_WAIT 5
/** State for a SOCKS connection: got a y.onion URL; waiting to receive
 * rendezvous descriptor. */
#define AP_CONN_STATE_RENDDESC_WAIT 6
/** The controller will attach this connection to a circuit; it isn't our
 * job to do so. */
#define AP_CONN_STATE_CONTROLLER_WAIT 7
/** State for a SOCKS connection: waiting for a completed circuit. */
#define AP_CONN_STATE_CIRCUIT_WAIT 8
/** State for a SOCKS connection: sent BEGIN, waiting for CONNECTED. */
#define AP_CONN_STATE_CONNECT_WAIT 9
/** State for a SOCKS connection: sent RESOLVE, waiting for RESOLVED. */
#define AP_CONN_STATE_RESOLVE_WAIT 10
/** State for a SOCKS connection: ready to send and receive. */
#define AP_CONN_STATE_OPEN 11
/** State for a transparent natd connection: waiting for original
 * destination. */
#define AP_CONN_STATE_NATD_WAIT 12
#define AP_CONN_STATE_MAX_ 12

/** True iff the AP_CONN_STATE_* value <b>s</b> means that the corresponding
 * edge connection is not attached to any circuit. */
#define AP_CONN_STATE_IS_UNATTACHED(s) \
  ((s) <= AP_CONN_STATE_CIRCUIT_WAIT || (s) == AP_CONN_STATE_NATD_WAIT)

#define DIR_CONN_STATE_MIN_ 1
/** State for connection to directory server: waiting for connect(). */
#define DIR_CONN_STATE_CONNECTING 1
/** State for connection to directory server: sending HTTP request. */
#define DIR_CONN_STATE_CLIENT_SENDING 2
/** State for connection to directory server: reading HTTP response. */
#define DIR_CONN_STATE_CLIENT_READING 3
/** State for connection to directory server: happy and finished. */
#define DIR_CONN_STATE_CLIENT_FINISHED 4
/** State for connection at directory server: waiting for HTTP request. */
#define DIR_CONN_STATE_SERVER_COMMAND_WAIT 5
/** State for connection at directory server: sending HTTP response. */
#define DIR_CONN_STATE_SERVER_WRITING 6
#define DIR_CONN_STATE_MAX_ 6

/** True iff the purpose of <b>conn</b> means that it's a server-side
 * directory connection. */
#define DIR_CONN_IS_SERVER(conn) ((conn)->purpose == DIR_PURPOSE_SERVER)

#define CONTROL_CONN_STATE_MIN_ 1
/** State for a control connection: Authenticated and accepting v1 commands. */
#define CONTROL_CONN_STATE_OPEN 1
/** State for a control connection: Waiting for authentication; speaking
 * protocol v1. */
#define CONTROL_CONN_STATE_NEEDAUTH 2
#define CONTROL_CONN_STATE_MAX_ 2

#define DIR_PURPOSE_MIN_ 4
/** A connection to a directory server: set after a v2 rendezvous
 * descriptor is downloaded. */
#define DIR_PURPOSE_HAS_FETCHED_RENDDESC_V2 4
/** A connection to a directory server: download one or more server
 * descriptors. */
#define DIR_PURPOSE_FETCH_SERVERDESC 6
/** A connection to a directory server: download one or more extra-info
 * documents. */
#define DIR_PURPOSE_FETCH_EXTRAINFO 7
/** A connection to a directory server: upload a server descriptor. */
#define DIR_PURPOSE_UPLOAD_DIR 8
/** A connection to a directory server: upload a v3 networkstatus vote. */
#define DIR_PURPOSE_UPLOAD_VOTE 10
/** A connection to a directory server: upload a v3 consensus signature */
#define DIR_PURPOSE_UPLOAD_SIGNATURES 11
/** A connection to a directory server: download one or more v3 networkstatus
 * votes. */
#define DIR_PURPOSE_FETCH_STATUS_VOTE 12
/** A connection to a directory server: download a v3 detached signatures
 * object for a consensus. */
#define DIR_PURPOSE_FETCH_DETACHED_SIGNATURES 13
/** A connection to a directory server: download a v3 networkstatus
 * consensus. */
#define DIR_PURPOSE_FETCH_CONSENSUS 14
/** A connection to a directory server: download one or more directory
 * authority certificates. */
#define DIR_PURPOSE_FETCH_CERTIFICATE 15

/** Purpose for connection at a directory server. */
#define DIR_PURPOSE_SERVER 16
/** A connection to a hidden service directory server: upload a v2 rendezvous
 * descriptor. */
#define DIR_PURPOSE_UPLOAD_RENDDESC_V2 17
/** A connection to a hidden service directory server: download a v2 rendezvous
 * descriptor. */
#define DIR_PURPOSE_FETCH_RENDDESC_V2 18
/** A connection to a directory server: download a microdescriptor. */
#define DIR_PURPOSE_FETCH_MICRODESC 19
#define DIR_PURPOSE_MAX_ 19

/** True iff <b>p</b> is a purpose corresponding to uploading data to a
 * directory server. */
#define DIR_PURPOSE_IS_UPLOAD(p)                \
  ((p)==DIR_PURPOSE_UPLOAD_DIR ||               \
   (p)==DIR_PURPOSE_UPLOAD_VOTE ||              \
   (p)==DIR_PURPOSE_UPLOAD_SIGNATURES)

#define EXIT_PURPOSE_MIN_ 1
/** This exit stream wants to do an ordinary connect. */
#define EXIT_PURPOSE_CONNECT 1
/** This exit stream wants to do a resolve (either normal or reverse). */
#define EXIT_PURPOSE_RESOLVE 2
#define EXIT_PURPOSE_MAX_ 2

/* !!!! If any connection purpose is ever over 31, we must grow the type
 * field in connection_t. */

/** Circuit state: I'm the origin, still haven't done all my handshakes. */
#define CIRCUIT_STATE_BUILDING 0
/** Circuit state: Waiting to process the onionskin. */
#define CIRCUIT_STATE_ONIONSKIN_PENDING 1
/** Circuit state: I'd like to deliver a create, but my n_chan is still
 * connecting. */
#define CIRCUIT_STATE_CHAN_WAIT 2
/** Circuit state: onionskin(s) processed, ready to send/receive cells. */
#define CIRCUIT_STATE_OPEN 3

#define CIRCUIT_PURPOSE_MIN_ 1

/* these circuits were initiated elsewhere */
#define CIRCUIT_PURPOSE_OR_MIN_ 1
/** OR-side circuit purpose: normal circuit, at OR. */
#define CIRCUIT_PURPOSE_OR 1
/** OR-side circuit purpose: At OR, from Bob, waiting for intro from Alices. */
#define CIRCUIT_PURPOSE_INTRO_POINT 2
/** OR-side circuit purpose: At OR, from Alice, waiting for Bob. */
#define CIRCUIT_PURPOSE_REND_POINT_WAITING 3
/** OR-side circuit purpose: At OR, both circuits have this purpose. */
#define CIRCUIT_PURPOSE_REND_ESTABLISHED 4
#define CIRCUIT_PURPOSE_OR_MAX_ 4

/* these circuits originate at this node */

/* here's how circ client-side purposes work:
 *   normal circuits are C_GENERAL.
 *   circuits that are c_introducing are either on their way to
 *     becoming open, or they are open and waiting for a
 *     suitable rendcirc before they send the intro.
 *   circuits that are c_introduce_ack_wait have sent the intro,
 *     but haven't gotten a response yet.
 *   circuits that are c_establish_rend are either on their way
 *     to becoming open, or they are open and have sent the
 *     establish_rendezvous cell but haven't received an ack.
 *   circuits that are c_rend_ready are open and have received a
 *     rend ack, but haven't heard from bob yet. if they have a
 *     buildstate->pending_final_cpath then they're expecting a
 *     cell from bob, else they're not.
 *   circuits that are c_rend_ready_intro_acked are open, and
 *     some intro circ has sent its intro and received an ack.
 *   circuits that are c_rend_joined are open, have heard from
 *     bob, and are talking to him.
 */
/** Client-side circuit purpose: Normal circuit, with cpath. */
#define CIRCUIT_PURPOSE_C_GENERAL 5
/** Client-side circuit purpose: at Alice, connecting to intro point. */
#define CIRCUIT_PURPOSE_C_INTRODUCING 6
/** Client-side circuit purpose: at Alice, sent INTRODUCE1 to intro point,
 * waiting for ACK/NAK. */
#define CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT 7
/** Client-side circuit purpose: at Alice, introduced and acked, closing. */
#define CIRCUIT_PURPOSE_C_INTRODUCE_ACKED 8
/** Client-side circuit purpose: at Alice, waiting for ack. */
#define CIRCUIT_PURPOSE_C_ESTABLISH_REND 9
/** Client-side circuit purpose: at Alice, waiting for Bob. */
#define CIRCUIT_PURPOSE_C_REND_READY 10
/** Client-side circuit purpose: at Alice, waiting for Bob, INTRODUCE
 * has been acknowledged. */
#define CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED 11
/** Client-side circuit purpose: at Alice, rendezvous established. */
#define CIRCUIT_PURPOSE_C_REND_JOINED 12
/** This circuit is used for build time measurement only */
#define CIRCUIT_PURPOSE_C_MEASURE_TIMEOUT 13
#define CIRCUIT_PURPOSE_C_MAX_ 13
/** Hidden-service-side circuit purpose: at Bob, waiting for introductions. */
#define CIRCUIT_PURPOSE_S_ESTABLISH_INTRO 14
/** Hidden-service-side circuit purpose: at Bob, successfully established
 * intro. */
#define CIRCUIT_PURPOSE_S_INTRO 15
/** Hidden-service-side circuit purpose: at Bob, connecting to rend point. */
#define CIRCUIT_PURPOSE_S_CONNECT_REND 16
/** Hidden-service-side circuit purpose: at Bob, rendezvous established. */
#define CIRCUIT_PURPOSE_S_REND_JOINED 17
/** A testing circuit; not meant to be used for actual traffic. */
#define CIRCUIT_PURPOSE_TESTING 18
/** A controller made this circuit and Tor should not use it. */
#define CIRCUIT_PURPOSE_CONTROLLER 19
/** This circuit is used for path bias probing only */
#define CIRCUIT_PURPOSE_PATH_BIAS_TESTING 20
#define CIRCUIT_PURPOSE_MAX_ 20
/** A catch-all for unrecognized purposes. Currently we don't expect
 * to make or see any circuits with this purpose. */
#define CIRCUIT_PURPOSE_UNKNOWN 255

/** True iff the circuit purpose <b>p</b> is for a circuit that
 * originated at this node. */
#define CIRCUIT_PURPOSE_IS_ORIGIN(p) ((p)>CIRCUIT_PURPOSE_OR_MAX_)
/** True iff the circuit purpose <b>p</b> is for a circuit that originated
 * here to serve as a client.  (Hidden services don't count here.) */
#define CIRCUIT_PURPOSE_IS_CLIENT(p)  \
  ((p)> CIRCUIT_PURPOSE_OR_MAX_ &&    \
   (p)<=CIRCUIT_PURPOSE_C_MAX_)
/** True iff the circuit_t <b>c</b> is actually an origin_circuit_t. */
#define CIRCUIT_IS_ORIGIN(c) (CIRCUIT_PURPOSE_IS_ORIGIN((c)->purpose))
/** True iff the circuit purpose <b>p</b> is for an established rendezvous
 * circuit. */
#define CIRCUIT_PURPOSE_IS_ESTABLISHED_REND(p) \
  ((p) == CIRCUIT_PURPOSE_C_REND_JOINED ||     \
   (p) == CIRCUIT_PURPOSE_S_REND_JOINED)
/** True iff the circuit_t c is actually an or_circuit_t */
#define CIRCUIT_IS_ORCIRC(c) (((circuit_t *)(c))->magic == OR_CIRCUIT_MAGIC)

/** How many circuits do we want simultaneously in-progress to handle
 * a given stream? */
#define MIN_CIRCUITS_HANDLING_STREAM 2

/* These RELAY_COMMAND constants define values for relay cell commands, and
* must match those defined in tor-spec.txt. */
#define RELAY_COMMAND_BEGIN 1
#define RELAY_COMMAND_DATA 2
#define RELAY_COMMAND_END 3
#define RELAY_COMMAND_CONNECTED 4
#define RELAY_COMMAND_SENDME 5
#define RELAY_COMMAND_EXTEND 6
#define RELAY_COMMAND_EXTENDED 7
#define RELAY_COMMAND_TRUNCATE 8
#define RELAY_COMMAND_TRUNCATED 9
#define RELAY_COMMAND_DROP 10
#define RELAY_COMMAND_RESOLVE 11
#define RELAY_COMMAND_RESOLVED 12
#define RELAY_COMMAND_BEGIN_DIR 13
#define RELAY_COMMAND_EXTEND2 14
#define RELAY_COMMAND_EXTENDED2 15

#define RELAY_COMMAND_ESTABLISH_INTRO 32
#define RELAY_COMMAND_ESTABLISH_RENDEZVOUS 33
#define RELAY_COMMAND_INTRODUCE1 34
#define RELAY_COMMAND_INTRODUCE2 35
#define RELAY_COMMAND_RENDEZVOUS1 36
#define RELAY_COMMAND_RENDEZVOUS2 37
#define RELAY_COMMAND_INTRO_ESTABLISHED 38
#define RELAY_COMMAND_RENDEZVOUS_ESTABLISHED 39
#define RELAY_COMMAND_INTRODUCE_ACK 40

/* Reasons why an OR connection is closed. */
#define END_OR_CONN_REASON_DONE           1
#define END_OR_CONN_REASON_REFUSED        2 /* connection refused */
#define END_OR_CONN_REASON_OR_IDENTITY    3
#define END_OR_CONN_REASON_CONNRESET      4 /* connection reset by peer */
#define END_OR_CONN_REASON_TIMEOUT        5
#define END_OR_CONN_REASON_NO_ROUTE       6 /* no route to host/net */
#define END_OR_CONN_REASON_IO_ERROR       7 /* read/write error */
#define END_OR_CONN_REASON_RESOURCE_LIMIT 8 /* sockets, buffers, etc */
#define END_OR_CONN_REASON_PT_MISSING     9 /* PT failed or not available */
#define END_OR_CONN_REASON_MISC           10

/* Reasons why we (or a remote OR) might close a stream. See tor-spec.txt for
 * documentation of these.  The values must match. */
#define END_STREAM_REASON_MISC 1
#define END_STREAM_REASON_RESOLVEFAILED 2
#define END_STREAM_REASON_CONNECTREFUSED 3
#define END_STREAM_REASON_EXITPOLICY 4
#define END_STREAM_REASON_DESTROY 5
#define END_STREAM_REASON_DONE 6
#define END_STREAM_REASON_TIMEOUT 7
#define END_STREAM_REASON_NOROUTE 8
#define END_STREAM_REASON_HIBERNATING 9
#define END_STREAM_REASON_INTERNAL 10
#define END_STREAM_REASON_RESOURCELIMIT 11
#define END_STREAM_REASON_CONNRESET 12
#define END_STREAM_REASON_TORPROTOCOL 13
#define END_STREAM_REASON_NOTDIRECTORY 14
#define END_STREAM_REASON_ENTRYPOLICY 15

/* These high-numbered end reasons are not part of the official spec,
 * and are not intended to be put in relay end cells. They are here
 * to be more informative when sending back socks replies to the
 * application. */
/* XXXX 256 is no longer used; feel free to reuse it. */
/** We were unable to attach the connection to any circuit at all. */
/* XXXX the ways we use this one don't make a lot of sense. */
#define END_STREAM_REASON_CANT_ATTACH 257
/** We can't connect to any directories at all, so we killed our streams
 * before they can time out. */
#define END_STREAM_REASON_NET_UNREACHABLE 258
/** This is a SOCKS connection, and the client used (or misused) the SOCKS
 * protocol in a way we couldn't handle. */
#define END_STREAM_REASON_SOCKSPROTOCOL 259
/** This is a transparent proxy connection, but we can't extract the original
 * target address:port. */
#define END_STREAM_REASON_CANT_FETCH_ORIG_DEST 260
/** This is a connection on the NATD port, and the destination IP:Port was
 * either ill-formed or out-of-range. */
#define END_STREAM_REASON_INVALID_NATD_DEST 261
/** The target address is in a private network (like 127.0.0.1 or 10.0.0.1);
 * you don't want to do that over a randomly chosen exit */
#define END_STREAM_REASON_PRIVATE_ADDR 262

/** Bitwise-and this value with endreason to mask out all flags. */
#define END_STREAM_REASON_MASK 511

/** Bitwise-or this with the argument to control_event_stream_status
 * to indicate that the reason came from an END cell. */
#define END_STREAM_REASON_FLAG_REMOTE 512
/** Bitwise-or this with the argument to control_event_stream_status
 * to indicate that we already sent a CLOSED stream event. */
#define END_STREAM_REASON_FLAG_ALREADY_SENT_CLOSED 1024
/** Bitwise-or this with endreason to indicate that we already sent
 * a socks reply, and no further reply needs to be sent from
 * connection_mark_unattached_ap(). */
#define END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED 2048

/** Reason for remapping an AP connection's address: we have a cached
 * answer. */
#define REMAP_STREAM_SOURCE_CACHE 1
/** Reason for remapping an AP connection's address: the exit node told us an
 * answer. */
#define REMAP_STREAM_SOURCE_EXIT 2

/* 'type' values to use in RESOLVED cells.  Specified in tor-spec.txt. */
#define RESOLVED_TYPE_HOSTNAME 0
#define RESOLVED_TYPE_IPV4 4
#define RESOLVED_TYPE_IPV6 6
#define RESOLVED_TYPE_ERROR_TRANSIENT 0xF0
#define RESOLVED_TYPE_ERROR 0xF1

/* Negative reasons are internal: we never send them in a DESTROY or TRUNCATE
 * call; they only go to the controller for tracking  */
/** Our post-timeout circuit time measurement period expired.
 * We must give up now */
#define END_CIRC_REASON_MEASUREMENT_EXPIRED -3

/** We couldn't build a path for this circuit. */
#define END_CIRC_REASON_NOPATH          -2
/** Catch-all "other" reason for closing origin circuits. */
#define END_CIRC_AT_ORIGIN              -1

/* Reasons why we (or a remote OR) might close a circuit. See tor-spec.txt for
 * documentation of these. */
#define END_CIRC_REASON_MIN_            0
#define END_CIRC_REASON_NONE            0
#define END_CIRC_REASON_TORPROTOCOL     1
#define END_CIRC_REASON_INTERNAL        2
#define END_CIRC_REASON_REQUESTED       3
#define END_CIRC_REASON_HIBERNATING     4
#define END_CIRC_REASON_RESOURCELIMIT   5
#define END_CIRC_REASON_CONNECTFAILED   6
#define END_CIRC_REASON_OR_IDENTITY     7
#define END_CIRC_REASON_CHANNEL_CLOSED  8
#define END_CIRC_REASON_FINISHED        9
#define END_CIRC_REASON_TIMEOUT         10
#define END_CIRC_REASON_DESTROYED       11
#define END_CIRC_REASON_NOSUCHSERVICE   12
#define END_CIRC_REASON_MAX_            12

/** Bitwise-OR this with the argument to circuit_mark_for_close() or
 * control_event_circuit_status() to indicate that the reason was
 * passed through from a destroy or truncate cell. */
#define END_CIRC_REASON_FLAG_REMOTE     512

/** Length of 'y' portion of 'y.onion' URL. */
#define REND_SERVICE_ID_LEN_BASE32 16

/** Length of 'y.onion' including '.onion' URL. */
#define REND_SERVICE_ADDRESS_LEN (16+1+5)

/** Length of a binary-encoded rendezvous service ID. */
#define REND_SERVICE_ID_LEN 10

/** Time period for which a v2 descriptor will be valid. */
#define REND_TIME_PERIOD_V2_DESC_VALIDITY (24*60*60)

/** Time period within which two sets of v2 descriptors will be uploaded in
 * parallel. */
#define REND_TIME_PERIOD_OVERLAPPING_V2_DESCS (60*60)

/** Number of non-consecutive replicas (i.e. distributed somewhere
 * in the ring) for a descriptor. */
#define REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS 2

/** Number of consecutive replicas for a descriptor. */
#define REND_NUMBER_OF_CONSECUTIVE_REPLICAS 3

/** Length of v2 descriptor ID (32 base32 chars = 160 bits). */
#define REND_DESC_ID_V2_LEN_BASE32 32

/** Length of the base32-encoded secret ID part of versioned hidden service
 * descriptors. */
#define REND_SECRET_ID_PART_LEN_BASE32 32

/** Length of the base32-encoded hash of an introduction point's
 * identity key. */
#define REND_INTRO_POINT_ID_LEN_BASE32 32

/** Length of the descriptor cookie that is used for client authorization
 * to hidden services. */
#define REND_DESC_COOKIE_LEN 16

/** Length of the base64-encoded descriptor cookie that is used for
 * exchanging client authorization between hidden service and client. */
#define REND_DESC_COOKIE_LEN_BASE64 22

/** Length of client identifier in encrypted introduction points for hidden
 * service authorization type 'basic'. */
#define REND_BASIC_AUTH_CLIENT_ID_LEN 4

/** Multiple of the number of clients to which the real number of clients
 * is padded with fake clients for hidden service authorization type
 * 'basic'. */
#define REND_BASIC_AUTH_CLIENT_MULTIPLE 16

/** Length of client entry consisting of client identifier and encrypted
 * session key for hidden service authorization type 'basic'. */
#define REND_BASIC_AUTH_CLIENT_ENTRY_LEN (REND_BASIC_AUTH_CLIENT_ID_LEN \
                                          + CIPHER_KEY_LEN)

/** Maximum size of v2 hidden service descriptors. */
#define REND_DESC_MAX_SIZE (20 * 1024)

/** Legal characters for use in authorized client names for a hidden
 * service. */
#define REND_LEGAL_CLIENTNAME_CHARACTERS \
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-_"

/** Maximum length of authorized client names for a hidden service. */
#define REND_CLIENTNAME_MAX_LEN 16

/** Length of the rendezvous cookie that is used to connect circuits at the
 * rendezvous point. */
#define REND_COOKIE_LEN DIGEST_LEN

/** Client authorization type that a hidden service performs. */
typedef enum rend_auth_type_t {
  REND_NO_AUTH      = 0,
  REND_BASIC_AUTH   = 1,
  REND_STEALTH_AUTH = 2,
} rend_auth_type_t;

/** Client-side configuration of authorization for a hidden service. */
typedef struct rend_service_authorization_t {
  char descriptor_cookie[REND_DESC_COOKIE_LEN];
  char onion_address[REND_SERVICE_ADDRESS_LEN+1];
  rend_auth_type_t auth_type;
} rend_service_authorization_t;

/** Client- and server-side data that is used for hidden service connection
 * establishment. Not all fields contain data depending on where this struct
 * is used. */
typedef struct rend_data_t {
  /** Onion address (without the .onion part) that a client requests. */
  char onion_address[REND_SERVICE_ID_LEN_BASE32+1];

  /** (Optional) descriptor cookie that is used by a client. */
  char descriptor_cookie[REND_DESC_COOKIE_LEN];

  /** Authorization type for accessing a service used by a client. */
  rend_auth_type_t auth_type;

  /** Hash of the hidden service's PK used by a service. */
  char rend_pk_digest[DIGEST_LEN];

  /** Rendezvous cookie used by both, client and service. */
  char rend_cookie[REND_COOKIE_LEN];
} rend_data_t;

/** Time interval for tracking replays of DH public keys received in
 * INTRODUCE2 cells.  Used only to avoid launching multiple
 * simultaneous attempts to connect to the same rendezvous point. */
#define REND_REPLAY_TIME_INTERVAL (5 * 60)

/** Used to indicate which way a cell is going on a circuit. */
typedef enum {
  CELL_DIRECTION_IN=1, /**< The cell is moving towards the origin. */
  CELL_DIRECTION_OUT=2, /**< The cell is moving away from the origin. */
} cell_direction_t;

/** Initial value for both sides of a circuit transmission window when the
 * circuit is initialized.  Measured in cells. */
#define CIRCWINDOW_START 1000
#define CIRCWINDOW_START_MIN 100
#define CIRCWINDOW_START_MAX 1000
/** Amount to increment a circuit window when we get a circuit SENDME. */
#define CIRCWINDOW_INCREMENT 100
/** Initial value on both sides of a stream transmission window when the
 * stream is initialized.  Measured in cells. */
#define STREAMWINDOW_START 500
/** Amount to increment a stream window when we get a stream SENDME. */
#define STREAMWINDOW_INCREMENT 50

/** Maximum number of queued cells on a circuit for which we are the
 * midpoint before we give up and kill it.  This must be >= circwindow
 * to avoid killing innocent circuits, and >= circwindow*2 to give
 * leaky-pipe a chance of working someday. The ORCIRC_MAX_MIDDLE_KILL_THRESH
 * ratio controls the margin of error between emitting a warning and
 * killing the circuit.
 */
#define ORCIRC_MAX_MIDDLE_CELLS (CIRCWINDOW_START_MAX*2)
/** Ratio of hard (circuit kill) to soft (warning) thresholds for the
 * ORCIRC_MAX_MIDDLE_CELLS tests.
 */
#define ORCIRC_MAX_MIDDLE_KILL_THRESH (1.1f)

/* Cell commands.  These values are defined in tor-spec.txt. */
#define CELL_PADDING 0
#define CELL_CREATE 1
#define CELL_CREATED 2
#define CELL_RELAY 3
#define CELL_DESTROY 4
#define CELL_CREATE_FAST 5
#define CELL_CREATED_FAST 6
#define CELL_VERSIONS 7
#define CELL_NETINFO 8
#define CELL_RELAY_EARLY 9
#define CELL_CREATE2 10
#define CELL_CREATED2 11

#define CELL_VPADDING 128
#define CELL_CERTS 129
#define CELL_AUTH_CHALLENGE 130
#define CELL_AUTHENTICATE 131
#define CELL_AUTHORIZE 132
#define CELL_COMMAND_MAX_ 132

/** How long to test reachability before complaining to the user. */
#define TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT (20*60)

/** Legal characters in a nickname. */
#define LEGAL_NICKNAME_CHARACTERS \
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

/** Name to use in client TLS certificates if no nickname is given. Once
 * Tor 0.1.2.x is obsolete, we can remove this. */
#define DEFAULT_CLIENT_NICKNAME "client"

/** Name chosen by routers that don't configure nicknames */
#define UNNAMED_ROUTER_NICKNAME "Unnamed"

/** Number of bytes in a SOCKS4 header. */
#define SOCKS4_NETWORK_LEN 8

/*
 * Relay payload:
 *         Relay command           [1 byte]
 *         Recognized              [2 bytes]
 *         Stream ID               [2 bytes]
 *         Partial SHA-1           [4 bytes]
 *         Length                  [2 bytes]
 *         Relay payload           [498 bytes]
 */

/** Number of bytes in a cell, minus cell header. */
#define CELL_PAYLOAD_SIZE 509
/** Number of bytes in a cell transmitted over the network, in the longest
 * form */
#define CELL_MAX_NETWORK_SIZE 514

/** Maximum length of a header on a variable-length cell. */
#define VAR_CELL_MAX_HEADER_SIZE 7

static int get_cell_network_size(int wide_circ_ids);
static INLINE int get_cell_network_size(int wide_circ_ids)
{
  return wide_circ_ids ? CELL_MAX_NETWORK_SIZE : CELL_MAX_NETWORK_SIZE - 2;
}
static int get_var_cell_header_size(int wide_circ_ids);
static INLINE int get_var_cell_header_size(int wide_circ_ids)
{
  return wide_circ_ids ? VAR_CELL_MAX_HEADER_SIZE :
    VAR_CELL_MAX_HEADER_SIZE - 2;
}
static int get_circ_id_size(int wide_circ_ids);
static INLINE int get_circ_id_size(int wide_circ_ids)
{
  return wide_circ_ids ? 4 : 2;
}

/** Number of bytes in a relay cell's header (not including general cell
 * header). */
#define RELAY_HEADER_SIZE (1+2+2+4+2)
/** Largest number of bytes that can fit in a relay cell payload. */
#define RELAY_PAYLOAD_SIZE (CELL_PAYLOAD_SIZE-RELAY_HEADER_SIZE)

/** Identifies a circuit on an or_connection */
typedef uint32_t circid_t;
/** Identifies a stream on a circuit */
typedef uint16_t streamid_t;

/* channel_t typedef; struct channel_s is in channel.h */

typedef struct channel_s channel_t;

/* channel_listener_t typedef; struct channel_listener_s is in channel.h */

typedef struct channel_listener_s channel_listener_t;

/* channel states for channel_t */

typedef enum {
  /*
   * Closed state - channel is inactive
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_CLOSING
   * Permitted transitions to:
   *   - CHANNEL_STATE_OPENING
   */
  CHANNEL_STATE_CLOSED = 0,
  /*
   * Opening state - channel is trying to connect
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_CLOSED
   * Permitted transitions to:
   *   - CHANNEL_STATE_CLOSING
   *   - CHANNEL_STATE_ERROR
   *   - CHANNEL_STATE_OPEN
   */
  CHANNEL_STATE_OPENING,
  /*
   * Open state - channel is active and ready for use
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_MAINT
   *   - CHANNEL_STATE_OPENING
   * Permitted transitions to:
   *   - CHANNEL_STATE_CLOSING
   *   - CHANNEL_STATE_ERROR
   *   - CHANNEL_STATE_MAINT
   */
  CHANNEL_STATE_OPEN,
  /*
   * Maintenance state - channel is temporarily offline for subclass specific
   *   maintenance activities such as TLS renegotiation.
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_OPEN
   * Permitted transitions to:
   *   - CHANNEL_STATE_CLOSING
   *   - CHANNEL_STATE_ERROR
   *   - CHANNEL_STATE_OPEN
   */
  CHANNEL_STATE_MAINT,
  /*
   * Closing state - channel is shutting down
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_MAINT
   *   - CHANNEL_STATE_OPEN
   * Permitted transitions to:
   *   - CHANNEL_STATE_CLOSED,
   *   - CHANNEL_STATE_ERROR
   */
  CHANNEL_STATE_CLOSING,
  /*
   * Error state - channel has experienced a permanent error
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_CLOSING
   *   - CHANNEL_STATE_MAINT
   *   - CHANNEL_STATE_OPENING
   *   - CHANNEL_STATE_OPEN
   * Permitted transitions to:
   *   - None
   */
  CHANNEL_STATE_ERROR,
  /*
   * Placeholder for maximum state value
   */
  CHANNEL_STATE_LAST
} channel_state_t;

/* channel listener states for channel_listener_t */

typedef enum {
  /*
   * Closed state - channel listener is inactive
   *
   * Permitted transitions from:
   *   - CHANNEL_LISTENER_STATE_CLOSING
   * Permitted transitions to:
   *   - CHANNEL_LISTENER_STATE_LISTENING
   */
  CHANNEL_LISTENER_STATE_CLOSED = 0,
  /*
   * Listening state - channel listener is listening for incoming
   * connections
   *
   * Permitted transitions from:
   *   - CHANNEL_LISTENER_STATE_CLOSED
   * Permitted transitions to:
   *   - CHANNEL_LISTENER_STATE_CLOSING
   *   - CHANNEL_LISTENER_STATE_ERROR
   */
  CHANNEL_LISTENER_STATE_LISTENING,
  /*
   * Closing state - channel listener is shutting down
   *
   * Permitted transitions from:
   *   - CHANNEL_LISTENER_STATE_LISTENING
   * Permitted transitions to:
   *   - CHANNEL_LISTENER_STATE_CLOSED,
   *   - CHANNEL_LISTENER_STATE_ERROR
   */
  CHANNEL_LISTENER_STATE_CLOSING,
  /*
   * Error state - channel listener has experienced a permanent error
   *
   * Permitted transitions from:
   *   - CHANNEL_STATE_CLOSING
   *   - CHANNEL_STATE_LISTENING
   * Permitted transitions to:
   *   - None
   */
  CHANNEL_LISTENER_STATE_ERROR,
  /*
   * Placeholder for maximum state value
   */
  CHANNEL_LISTENER_STATE_LAST
} channel_listener_state_t;

/* TLS channel stuff */

typedef struct channel_tls_s channel_tls_t;

/* circuitmux_t typedef; struct circuitmux_s is in circuitmux.h */

typedef struct circuitmux_s circuitmux_t;

/** Parsed onion routing cell.  All communication between nodes
 * is via cells. */
typedef struct cell_t {
  circid_t circ_id; /**< Circuit which received the cell. */
  uint8_t command; /**< Type of the cell: one of CELL_PADDING, CELL_CREATE,
                    * CELL_DESTROY, etc */
  uint8_t payload[CELL_PAYLOAD_SIZE]; /**< Cell body. */
} cell_t;

/** Parsed variable-length onion routing cell. */
typedef struct var_cell_t {
  /** Type of the cell: CELL_VERSIONS, etc. */
  uint8_t command;
  /** Circuit thich received the cell */
  circid_t circ_id;
  /** Number of bytes actually stored in <b>payload</b> */
  uint16_t payload_len;
  /** Payload of this cell */
  uint8_t payload[FLEXIBLE_ARRAY_MEMBER];
} var_cell_t;

/** A parsed Extended ORPort message. */
typedef struct ext_or_cmd_t {
  uint16_t cmd; /** Command type */
  uint16_t len; /** Body length */
  char body[FLEXIBLE_ARRAY_MEMBER]; /** Message body */
} ext_or_cmd_t;

/** A cell as packed for writing to the network. */
typedef struct packed_cell_t {
  /** Next cell queued on this circuit. */
  TOR_SIMPLEQ_ENTRY(packed_cell_t) next;
  char body[CELL_MAX_NETWORK_SIZE]; /**< Cell as packed for network. */
  uint32_t inserted_time; /**< Time (in milliseconds since epoch, with high
                           * bits truncated) when this cell was inserted. */
} packed_cell_t;

/** A queue of cells on a circuit, waiting to be added to the
 * or_connection_t's outbuf. */
typedef struct cell_queue_t {
  /** Linked list of packed_cell_t*/
  TOR_SIMPLEQ_HEAD(cell_simpleq, packed_cell_t) head;
  int n; /**< The number of cells in the queue. */
} cell_queue_t;

/** Beginning of a RELAY cell payload. */
typedef struct {
  uint8_t command; /**< The end-to-end relay command. */
  uint16_t recognized; /**< Used to tell whether cell is for us. */
  streamid_t stream_id; /**< Which stream is this cell associated with? */
  char integrity[4]; /**< Used to tell whether cell is corrupted. */
  uint16_t length; /**< How long is the payload body? */
} relay_header_t;

typedef struct buf_t buf_t;
typedef struct socks_request_t socks_request_t;
#ifdef USE_BUFFEREVENTS
#define generic_buffer_t struct evbuffer
#else
#define generic_buffer_t buf_t
#endif

/* Values for connection_t.magic: used to make sure that downcasts (casts from
* connection_t to foo_connection_t) are safe. */
#define BASE_CONNECTION_MAGIC 0x7C3C304Eu
#define OR_CONNECTION_MAGIC 0x7D31FF03u
#define EDGE_CONNECTION_MAGIC 0xF0374013u
#define ENTRY_CONNECTION_MAGIC 0xbb4a5703
#define DIR_CONNECTION_MAGIC 0x9988ffeeu
#define CONTROL_CONNECTION_MAGIC 0x8abc765du
#define LISTENER_CONNECTION_MAGIC 0x1a1ac741u

/** Description of a connection to another host or process, and associated
 * data.
 *
 * A connection is named based on what it's connected to -- an "OR
 * connection" has a Tor node on the other end, an "exit
 * connection" has a website or other server on the other end, and an
 * "AP connection" has an application proxy (and thus a user) on the
 * other end.
 *
 * Every connection has a type and a state.  Connections never change
 * their type, but can go through many state changes in their lifetime.
 *
 * Every connection has two associated input and output buffers.
 * Listeners don't use them.  For non-listener connections, incoming
 * data is appended to conn->inbuf, and outgoing data is taken from
 * conn->outbuf.  Connections differ primarily in the functions called
 * to fill and drain these buffers.
 */
typedef struct connection_t {
  uint32_t magic; /**< For memory debugging: must equal one of
                   * *_CONNECTION_MAGIC. */

  uint8_t state; /**< Current state of this connection. */
  unsigned int type:5; /**< What kind of connection is this? */
  unsigned int purpose:5; /**< Only used for DIR and EXIT types currently. */

  /* The next fields are all one-bit booleans. Some are only applicable to
   * connection subtypes, but we hold them here anyway, to save space.
   */
  unsigned int read_blocked_on_bw:1; /**< Boolean: should we start reading
                            * again once the bandwidth throttler allows it? */
  unsigned int write_blocked_on_bw:1; /**< Boolean: should we start writing
                             * again once the bandwidth throttler allows
                             * writes? */
  unsigned int hold_open_until_flushed:1; /**< Despite this connection's being
                                      * marked for close, do we flush it
                                      * before closing it? */
  unsigned int inbuf_reached_eof:1; /**< Boolean: did read() return 0 on this
                                     * conn? */
  /** Set to 1 when we're inside connection_flushed_some to keep us from
   * calling connection_handle_write() recursively. */
  unsigned int in_flushed_some:1;
  /** True if connection_handle_write is currently running on this connection.
   */
  unsigned int in_connection_handle_write:1;

  /* For linked connections:
   */
  unsigned int linked:1; /**< True if there is, or has been, a linked_conn. */
  /** True iff we'd like to be notified about read events from the
   * linked conn. */
  unsigned int reading_from_linked_conn:1;
  /** True iff we're willing to write to the linked conn. */
  unsigned int writing_to_linked_conn:1;
  /** True iff we're currently able to read on the linked conn, and our
   * read_event should be made active with libevent. */
  unsigned int active_on_link:1;
  /** True iff we've called connection_close_immediate() on this linked
   * connection. */
  unsigned int linked_conn_is_closed:1;

  /** CONNECT/SOCKS proxy client handshake state (for outgoing connections). */
  unsigned int proxy_state:4;

  /** Our socket; set to TOR_INVALID_SOCKET if this connection is closed,
   * or has no socket. */
  tor_socket_t s;
  int conn_array_index; /**< Index into the global connection array. */

  struct event *read_event; /**< Libevent event structure. */
  struct event *write_event; /**< Libevent event structure. */
  buf_t *inbuf; /**< Buffer holding data read over this connection. */
  buf_t *outbuf; /**< Buffer holding data to write over this connection. */
  size_t outbuf_flushlen; /**< How much data should we try to flush from the
                           * outbuf? */
  time_t timestamp_lastread; /**< When was the last time libevent said we could
                              * read? */
  time_t timestamp_lastwritten; /**< When was the last time libevent said we
                                 * could write? */

#ifdef USE_BUFFEREVENTS
  struct bufferevent *bufev; /**< A Libevent buffered IO structure. */
#endif

  time_t timestamp_created; /**< When was this connection_t created? */

  /* XXXX_IP6 make this IPv6-capable */
  int socket_family; /**< Address family of this connection's socket.  Usually
                      * AF_INET, but it can also be AF_UNIX, or in the future
                      * AF_INET6 */
  tor_addr_t addr; /**< IP of the other side of the connection; used to
                    * identify routers, along with port. */
  uint16_t port; /**< If non-zero, port on the other end
                  * of the connection. */
  uint16_t marked_for_close; /**< Should we close this conn on the next
                              * iteration of the main loop? (If true, holds
                              * the line number where this connection was
                              * marked.) */
  const char *marked_for_close_file; /**< For debugging: in which file were
                                      * we marked for close? */
  char *address; /**< FQDN (or IP) of the guy on the other end.
                  * strdup into this, because free_connection() frees it. */
  /** Another connection that's connected to this one in lieu of a socket. */
  struct connection_t *linked_conn;

  /** Unique identifier for this connection on this Tor instance. */
  uint64_t global_identifier;

  /** Bytes read since last call to control_event_conn_bandwidth_used().
   * Only used if we're configured to emit CONN_BW events. */
  uint32_t n_read_conn_bw;

  /** Bytes written since last call to control_event_conn_bandwidth_used().
   * Only used if we're configured to emit CONN_BW events. */
  uint32_t n_written_conn_bw;
} connection_t;

/** Subtype of connection_t; used for a listener socket. */
typedef struct listener_connection_t {
  connection_t base_;

  /** If the connection is a CONN_TYPE_AP_DNS_LISTENER, this field points
   * to the evdns_server_port it uses to listen to and answer connections. */
  struct evdns_server_port *dns_server_port;

  /** @name Isolation parameters
   *
   * For an AP listener, these fields describe how to isolate streams that
   * arrive on the listener.
   *
   * @{
   */
  /** The session group for this listener. */
  int session_group;
  /** One or more ISO_ flags to describe how to isolate streams. */
  uint8_t isolation_flags;
  /**@}*/
  /** For SOCKS connections only: If this is set, we will choose "no
   * authentication" instead of "username/password" authentication if both
   * are offered. Used as input to parse_socks. */
  unsigned int socks_prefer_no_auth : 1;

  /** For a SOCKS listeners, these fields describe whether we should
   * allow IPv4 and IPv6 addresses from our exit nodes, respectively.
   *
   * @{
   */
  unsigned int socks_ipv4_traffic : 1;
  unsigned int socks_ipv6_traffic : 1;
  /** @} */
  /** For a socks listener: should we tell the exit that we prefer IPv6
   * addresses? */
  unsigned int socks_prefer_ipv6 : 1;

  /** For a socks listener: should we cache IPv4/IPv6 DNS information that
   * exit nodes tell us?
   *
   * @{ */
  unsigned int cache_ipv4_answers : 1;
  unsigned int cache_ipv6_answers : 1;
  /** @} */
  /** For a socks listeners: if we find an answer in our client-side DNS cache,
   * should we use it?
   *
   * @{ */
  unsigned int use_cached_ipv4_answers : 1;
  unsigned int use_cached_ipv6_answers : 1;
  /** @} */
  /** For socks listeners: When we can automap an address to IPv4 or IPv6,
   * do we prefer IPv6? */
  unsigned int prefer_ipv6_virtaddr : 1;

} listener_connection_t;

/** Minimum length of the random part of an AUTH_CHALLENGE cell. */
#define OR_AUTH_CHALLENGE_LEN 32

/**
 * @name Certificate types for CERTS cells.
 *
 * These values are defined by the protocol, and affect how an X509
 * certificate in a CERTS cell is interpreted and used.
 *
 * @{ */
/** A certificate that authenticates a TLS link key.  The subject key
 * must match the key used in the TLS handshake; it must be signed by
 * the identity key. */
#define OR_CERT_TYPE_TLS_LINK 1
/** A self-signed identity certificate. The subject key must be a
 * 1024-bit RSA key. */
#define OR_CERT_TYPE_ID_1024 2
/** A certificate that authenticates a key used in an AUTHENTICATE cell
 * in the v3 handshake.  The subject key must be a 1024-bit RSA key; it
 * must be signed by the identity key */
#define OR_CERT_TYPE_AUTH_1024 3
/**@}*/

/** The one currently supported type of AUTHENTICATE cell.  It contains
 * a bunch of structures signed with an RSA1024 key.  The signed
 * structures include a HMAC using negotiated TLS secrets, and a digest
 * of all cells sent or received before the AUTHENTICATE cell (including
 * the random server-generated AUTH_CHALLENGE cell).
 */
#define AUTHTYPE_RSA_SHA256_TLSSECRET 1

/** The length of the part of the AUTHENTICATE cell body that the client and
 * server can generate independently (when using RSA_SHA256_TLSSECRET). It
 * contains everything except the client's timestamp, the client's randomly
 * generated nonce, and the signature. */
#define V3_AUTH_FIXED_PART_LEN (8+(32*6))
/** The length of the part of the AUTHENTICATE cell body that the client
 * signs. */
#define V3_AUTH_BODY_LEN (V3_AUTH_FIXED_PART_LEN + 8 + 16)

/** Stores flags and information related to the portion of a v2/v3 Tor OR
 * connection handshake that happens after the TLS handshake is finished.
 */
typedef struct or_handshake_state_t {
  /** When was the VERSIONS cell sent on this connection?  Used to get
   * an estimate of the skew in the returning NETINFO reply. */
  time_t sent_versions_at;
  /** True iff we originated this connection */
  unsigned int started_here : 1;
  /** True iff we have received and processed a VERSIONS cell. */
  unsigned int received_versions : 1;
  /** True iff we have received and processed an AUTH_CHALLENGE cell */
  unsigned int received_auth_challenge : 1;
  /** True iff we have received and processed a CERTS cell. */
  unsigned int received_certs_cell : 1;
  /** True iff we have received and processed an AUTHENTICATE cell */
  unsigned int received_authenticate : 1;

  /* True iff we've received valid authentication to some identity. */
  unsigned int authenticated : 1;

  /* True iff we have sent a netinfo cell */
  unsigned int sent_netinfo : 1;

  /** True iff we should feed outgoing cells into digest_sent and
   * digest_received respectively.
   *
   * From the server's side of the v3 handshake, we want to capture everything
   * from the VERSIONS cell through and including the AUTH_CHALLENGE cell.
   * From the client's, we want to capture everything from the VERSIONS cell
   * through but *not* including the AUTHENTICATE cell.
   *
   * @{ */
  unsigned int digest_sent_data : 1;
  unsigned int digest_received_data : 1;
  /**@}*/

  /** Identity digest that we have received and authenticated for our peer
   * on this connection. */
  uint8_t authenticated_peer_id[DIGEST_LEN];

  /** Digests of the cells that we have sent or received as part of a V3
   * handshake.  Used for making and checking AUTHENTICATE cells.
   *
   * @{
   */
  crypto_digest_t *digest_sent;
  crypto_digest_t *digest_received;
  /** @} */

  /** Certificates that a connection initiator sent us in a CERTS cell; we're
   * holding on to them until we get an AUTHENTICATE cell.
   *
   * @{
   */
  /** The cert for the key that's supposed to sign the AUTHENTICATE cell */
  tor_cert_t *auth_cert;
  /** A self-signed identity certificate */
  tor_cert_t *id_cert;
  /**@}*/
} or_handshake_state_t;

/** Length of Extended ORPort connection identifier. */
#define EXT_OR_CONN_ID_LEN DIGEST_LEN /* 20 */

/** Subtype of connection_t for an "OR connection" -- that is, one that speaks
 * cells over TLS. */
typedef struct or_connection_t {
  connection_t base_;

  /** Hash of the public RSA key for the other side's identity key, or zeroes
   * if the other side hasn't shown us a valid identity key. */
  char identity_digest[DIGEST_LEN];

  /** Extended ORPort connection identifier. */
  char *ext_or_conn_id;
  /** This is the ClientHash value we expect to receive from the
   *  client during the Extended ORPort authentication protocol. We
   *  compute it upon receiving the ClientNoce from the client, and we
   *  compare it with the acual ClientHash value sent by the
   *  client. */
  char *ext_or_auth_correct_client_hash;
  /** String carrying the name of the pluggable transport
   *  (e.g. "obfs2") that is obfuscating this connection. If no
   *  pluggable transports are used, it's NULL. */
  char *ext_or_transport;

  char *nickname; /**< Nickname of OR on other side (if any). */

  tor_tls_t *tls; /**< TLS connection state. */
  int tls_error; /**< Last tor_tls error code. */
  /** When we last used this conn for any client traffic. If not
   * recent, we can rate limit it further. */

  /* Channel using this connection */
  channel_tls_t *chan;

  tor_addr_t real_addr; /**< The actual address that this connection came from
                       * or went to.  The <b>addr</b> field is prone to
                       * getting overridden by the address from the router
                       * descriptor matching <b>identity_digest</b>. */

  /** Should this connection be used for extending circuits to the server
   * matching the <b>identity_digest</b> field?  Set to true if we're pretty
   * sure we aren't getting MITMed, either because we're connected to an
   * address listed in a server descriptor, or because an authenticated
   * NETINFO cell listed the address we're connected to as recognized. */
  unsigned int is_canonical:1;

  /** True iff we have decided that the other end of this connection
   * is a client.  Connections with this flag set should never be used
   * to satisfy an EXTEND request.  */
  unsigned int is_connection_with_client:1;
  /** True iff this is an outgoing connection. */
  unsigned int is_outgoing:1;
  unsigned int proxy_type:2; /**< One of PROXY_NONE...PROXY_SOCKS5 */
  unsigned int wide_circ_ids:1;
  /** True iff this connection has had its bootstrap failure logged with
   * control_event_bootstrap_problem. */
  unsigned int have_noted_bootstrap_problem:1;

  uint16_t link_proto; /**< What protocol version are we using? 0 for
                        * "none negotiated yet." */
  uint16_t idle_timeout; /**< How long can this connection sit with no
                          * circuits on it before we close it? Based on
                          * IDLE_CIRCUIT_TIMEOUT_{NON,}CANONICAL and
                          * on is_canonical, randomized. */
  or_handshake_state_t *handshake_state; /**< If we are setting this connection
                                          * up, state information to do so. */

  time_t timestamp_lastempty; /**< When was the outbuf last completely empty?*/

  /* bandwidth* and *_bucket only used by ORs in OPEN state: */
  int bandwidthrate; /**< Bytes/s added to the bucket. (OPEN ORs only.) */
  int bandwidthburst; /**< Max bucket size for this conn. (OPEN ORs only.) */
#ifndef USE_BUFFEREVENTS
  int read_bucket; /**< When this hits 0, stop receiving. Every second we
                    * add 'bandwidthrate' to this, capping it at
                    * bandwidthburst. (OPEN ORs only) */
  int write_bucket; /**< When this hits 0, stop writing. Like read_bucket. */
#else
  /** A rate-limiting configuration object to determine how this connection
   * set its read- and write- limits. */
  /* XXXX we could share this among all connections. */
  struct ev_token_bucket_cfg *bucket_cfg;
#endif

  struct or_connection_t *next_with_same_id; /**< Next connection with same
                                              * identity digest as this one. */
  /** Last emptied read token bucket in msec since midnight; only used if
   * TB_EMPTY events are enabled. */
  uint32_t read_emptied_time;
  /** Last emptied write token bucket in msec since midnight; only used if
   * TB_EMPTY events are enabled. */
  uint32_t write_emptied_time;
} or_connection_t;

/** Subtype of connection_t for an "edge connection" -- that is, an entry (ap)
 * connection, or an exit. */
typedef struct edge_connection_t {
  connection_t base_;

  struct edge_connection_t *next_stream; /**< Points to the next stream at this
                                          * edge, if any */
  int package_window; /**< How many more relay cells can I send into the
                       * circuit? */
  int deliver_window; /**< How many more relay cells can end at me? */

  struct circuit_t *on_circuit; /**< The circuit (if any) that this edge
                                 * connection is using. */

  /** A pointer to which node in the circ this conn exits at.  Set for AP
   * connections and for hidden service exit connections. */
  struct crypt_path_t *cpath_layer;
  /** What rendezvous service are we querying for (if an AP) or providing (if
   * an exit)? */
  rend_data_t *rend_data;

  uint32_t address_ttl; /**< TTL for address-to-addr mapping on exit
                         * connection.  Exit connections only. */
  uint32_t begincell_flags; /** Flags sent or received in the BEGIN cell
                             * for this connection */

  streamid_t stream_id; /**< The stream ID used for this edge connection on its
                         * circuit */

  /** The reason why this connection is closing; passed to the controller. */
  uint16_t end_reason;

  /** Bytes read since last call to control_event_stream_bandwidth_used() */
  uint32_t n_read;

  /** Bytes written since last call to control_event_stream_bandwidth_used() */
  uint32_t n_written;

  /** True iff this connection is for a DNS request only. */
  unsigned int is_dns_request:1;
  /** True iff this connection is for a PTR DNS request. (exit only) */
  unsigned int is_reverse_dns_lookup:1;

  unsigned int edge_has_sent_end:1; /**< For debugging; only used on edge
                         * connections.  Set once we've set the stream end,
                         * and check in connection_about_to_close_connection().
                         */
  /** True iff we've blocked reading until the circuit has fewer queued
   * cells. */
  unsigned int edge_blocked_on_circ:1;

  /** Unique ID for directory requests; this used to be in connection_t, but
   * that's going away and being used on channels instead.  We still tag
   * edge connections with dirreq_id from circuits, so it's copied here. */
  uint64_t dirreq_id;
} edge_connection_t;

/** Subtype of edge_connection_t for an "entry connection" -- that is, a SOCKS
 * connection, a DNS request, a TransPort connection or a NATD connection */
typedef struct entry_connection_t {
  edge_connection_t edge_;

  /** Nickname of planned exit node -- used with .exit support. */
  char *chosen_exit_name;

  socks_request_t *socks_request; /**< SOCKS structure describing request (AP
                                   * only.) */

  /* === Isolation related, AP only. === */
  /** AP only: based on which factors do we isolate this stream? */
  uint8_t isolation_flags;
  /** AP only: what session group is this stream in? */
  int session_group;
  /** AP only: The newnym epoch in which we created this connection. */
  unsigned nym_epoch;
  /** AP only: The original requested address before we rewrote it. */
  char *original_dest_address;
  /* Other fields to isolate on already exist.  The ClientAddr is addr.  The
     ClientProtocol is a combination of type and socks_request->
     socks_version.  SocksAuth is socks_request->username/password.
     DestAddr is in socks_request->address. */

  /** Number of times we've reassigned this application connection to
   * a new circuit. We keep track because the timeout is longer if we've
   * already retried several times. */
  uint8_t num_socks_retries;

  /** For AP connections only: buffer for data that we have sent
   * optimistically, which we might need to re-send if we have to
   * retry this connection. */
  generic_buffer_t *pending_optimistic_data;
  /* For AP connections only: buffer for data that we previously sent
  * optimistically which we are currently re-sending as we retry this
  * connection. */
  generic_buffer_t *sending_optimistic_data;

  /** If this is a DNSPort connection, this field holds the pending DNS
   * request that we're going to try to answer.  */
  struct evdns_server_request *dns_server_request;

#define NUM_CIRCUITS_LAUNCHED_THRESHOLD 10
  /** Number of times we've launched a circuit to handle this stream. If
    * it gets too high, that could indicate an inconsistency between our
    * "launch a circuit to handle this stream" logic and our "attach our
    * stream to one of the available circuits" logic. */
  unsigned int num_circuits_launched:4;

  /** True iff this stream must attach to a one-hop circuit (e.g. for
   * begin_dir). */
  unsigned int want_onehop:1;
  /** True iff this stream should use a BEGIN_DIR relay command to establish
   * itself rather than BEGIN (either via onehop or via a whole circuit). */
  unsigned int use_begindir:1;

  /** For AP connections only. If 1, and we fail to reach the chosen exit,
   * stop requiring it. */
  unsigned int chosen_exit_optional:1;
  /** For AP connections only. If non-zero, this exit node was picked as
   * a result of the TrackHostExit, and the value decrements every time
   * we fail to complete a circuit to our chosen exit -- if it reaches
   * zero, abandon the associated mapaddress. */
  unsigned int chosen_exit_retries:3;

  /** True iff this is an AP connection that came from a transparent or
   * NATd connection */
  unsigned int is_transparent_ap:1;

  /** For AP connections only: Set if this connection's target exit node
   * allows optimistic data (that is, data sent on this stream before
   * the exit has sent a CONNECTED cell) and we have chosen to use it.
   */
  unsigned int may_use_optimistic_data : 1;

  /** Should we permit IPv4 and IPv6 traffic to use this connection?
   *
   * @{ */
  unsigned int ipv4_traffic_ok : 1;
  unsigned int ipv6_traffic_ok : 1;
  /** @} */
  /** Should we say we prefer IPv6 traffic? */
  unsigned int prefer_ipv6_traffic : 1;

  /** For a socks listener: should we cache IPv4/IPv6 DNS information that
   * exit nodes tell us?
   *
   * @{ */
  unsigned int cache_ipv4_answers : 1;
  unsigned int cache_ipv6_answers : 1;
  /** @} */
  /** For a socks listeners: if we find an answer in our client-side DNS cache,
   * should we use it?
   *
   * @{ */
  unsigned int use_cached_ipv4_answers : 1;
  unsigned int use_cached_ipv6_answers : 1;
  /** @} */
  /** For socks listeners: When we can automap an address to IPv4 or IPv6,
   * do we prefer IPv6? */
  unsigned int prefer_ipv6_virtaddr : 1;

} entry_connection_t;

typedef enum {
    DIR_SPOOL_NONE=0, DIR_SPOOL_SERVER_BY_DIGEST, DIR_SPOOL_SERVER_BY_FP,
    DIR_SPOOL_EXTRA_BY_DIGEST, DIR_SPOOL_EXTRA_BY_FP,
    DIR_SPOOL_CACHED_DIR, DIR_SPOOL_NETWORKSTATUS,
    DIR_SPOOL_MICRODESC, /* NOTE: if we add another entry, add another bit. */
} dir_spool_source_t;
#define dir_spool_source_bitfield_t ENUM_BF(dir_spool_source_t)

/** Subtype of connection_t for an "directory connection" -- that is, an HTTP
 * connection to retrieve or serve directory material. */
typedef struct dir_connection_t {
  connection_t base_;

 /** Which 'resource' did we ask the directory for? This is typically the part
  * of the URL string that defines, relative to the directory conn purpose,
  * what thing we want.  For example, in router descriptor downloads by
  * descriptor digest, it contains "d/", then one ore more +-separated
  * fingerprints.
  **/
  char *requested_resource;
  unsigned int dirconn_direct:1; /**< Is this dirconn direct, or via Tor? */

  /* Used only for server sides of some dir connections, to implement
   * "spooling" of directory material to the outbuf.  Otherwise, we'd have
   * to append everything to the outbuf in one enormous chunk. */
  /** What exactly are we spooling right now? */
  dir_spool_source_bitfield_t  dir_spool_src : 3;

  /** If we're fetching descriptors, what router purpose shall we assign
   * to them? */
  uint8_t router_purpose;
  /** List of fingerprints for networkstatuses or descriptors to be spooled. */
  smartlist_t *fingerprint_stack;
  /** A cached_dir_t object that we're currently spooling out */
  struct cached_dir_t *cached_dir;
  /** The current offset into cached_dir. */
  off_t cached_dir_offset;
  /** The zlib object doing on-the-fly compression for spooled data. */
  tor_zlib_state_t *zlib_state;

  /** What rendezvous service are we querying for? */
  rend_data_t *rend_data;

  char identity_digest[DIGEST_LEN]; /**< Hash of the public RSA key for
                                     * the directory server's signing key. */

  /** Unique ID for directory requests; this used to be in connection_t, but
   * that's going away and being used on channels instead.  The dirserver still
   * needs this for the incoming side, so it's moved here. */
  uint64_t dirreq_id;
} dir_connection_t;

/** Subtype of connection_t for an connection to a controller. */
typedef struct control_connection_t {
  connection_t base_;

  uint64_t event_mask; /**< Bitfield: which events does this controller
                        * care about?
                        * EVENT_MAX_ is >31, so we need a 64 bit mask */

  /** True if we have sent a protocolinfo reply on this connection. */
  unsigned int have_sent_protocolinfo:1;
  /** True if we have received a takeownership command on this
   * connection. */
  unsigned int is_owning_control_connection:1;

  /** If we have sent an AUTHCHALLENGE reply on this connection and
   * have not received a successful AUTHENTICATE command, points to
   * the value which the client must send to authenticate itself;
   * otherwise, NULL. */
  char *safecookie_client_hash;

  /** Amount of space allocated in incoming_cmd. */
  uint32_t incoming_cmd_len;
  /** Number of bytes currently stored in incoming_cmd. */
  uint32_t incoming_cmd_cur_len;
  /** A control command that we're reading from the inbuf, but which has not
   * yet arrived completely. */
  char *incoming_cmd;
} control_connection_t;

/** Cast a connection_t subtype pointer to a connection_t **/
#define TO_CONN(c) (&(((c)->base_)))
/** Helper macro: Given a pointer to to.base_, of type from*, return &to. */
#define DOWNCAST(to, ptr) ((to*)SUBTYPE_P(ptr, to, base_))

/** Cast a entry_connection_t subtype pointer to a edge_connection_t **/
#define ENTRY_TO_EDGE_CONN(c) (&(((c))->edge_))
/** Cast a entry_connection_t subtype pointer to a connection_t **/
#define ENTRY_TO_CONN(c) (TO_CONN(ENTRY_TO_EDGE_CONN(c)))

/** Convert a connection_t* to an or_connection_t*; assert if the cast is
 * invalid. */
static or_connection_t *TO_OR_CONN(connection_t *);
/** Convert a connection_t* to a dir_connection_t*; assert if the cast is
 * invalid. */
static dir_connection_t *TO_DIR_CONN(connection_t *);
/** Convert a connection_t* to an edge_connection_t*; assert if the cast is
 * invalid. */
static edge_connection_t *TO_EDGE_CONN(connection_t *);
/** Convert a connection_t* to an entry_connection_t*; assert if the cast is
 * invalid. */
static entry_connection_t *TO_ENTRY_CONN(connection_t *);
/** Convert a edge_connection_t* to an entry_connection_t*; assert if the cast
 * is invalid. */
static entry_connection_t *EDGE_TO_ENTRY_CONN(edge_connection_t *);
/** Convert a connection_t* to an control_connection_t*; assert if the cast is
 * invalid. */
static control_connection_t *TO_CONTROL_CONN(connection_t *);
/** Convert a connection_t* to an listener_connection_t*; assert if the cast is
 * invalid. */
static listener_connection_t *TO_LISTENER_CONN(connection_t *);

static INLINE or_connection_t *TO_OR_CONN(connection_t *c)
{
  tor_assert(c->magic == OR_CONNECTION_MAGIC);
  return DOWNCAST(or_connection_t, c);
}
static INLINE dir_connection_t *TO_DIR_CONN(connection_t *c)
{
  tor_assert(c->magic == DIR_CONNECTION_MAGIC);
  return DOWNCAST(dir_connection_t, c);
}
static INLINE edge_connection_t *TO_EDGE_CONN(connection_t *c)
{
  tor_assert(c->magic == EDGE_CONNECTION_MAGIC ||
             c->magic == ENTRY_CONNECTION_MAGIC);
  return DOWNCAST(edge_connection_t, c);
}
static INLINE entry_connection_t *TO_ENTRY_CONN(connection_t *c)
{
  tor_assert(c->magic == ENTRY_CONNECTION_MAGIC);
  return (entry_connection_t*) SUBTYPE_P(c, entry_connection_t, edge_.base_);
}
static INLINE entry_connection_t *EDGE_TO_ENTRY_CONN(edge_connection_t *c)
{
  tor_assert(c->base_.magic == ENTRY_CONNECTION_MAGIC);
  return (entry_connection_t*) SUBTYPE_P(c, entry_connection_t, edge_);
}
static INLINE control_connection_t *TO_CONTROL_CONN(connection_t *c)
{
  tor_assert(c->magic == CONTROL_CONNECTION_MAGIC);
  return DOWNCAST(control_connection_t, c);
}
static INLINE listener_connection_t *TO_LISTENER_CONN(connection_t *c)
{
  tor_assert(c->magic == LISTENER_CONNECTION_MAGIC);
  return DOWNCAST(listener_connection_t, c);
}

/* Conditional macros to help write code that works whether bufferevents are
   disabled or not.

   We can't just write:
      if (conn->bufev) {
        do bufferevent stuff;
      } else {
        do other stuff;
      }
   because the bufferevent stuff won't even compile unless we have a fairly
   new version of Libevent.  Instead, we say:
      IF_HAS_BUFFEREVENT(conn, { do_bufferevent_stuff } );
   or:
      IF_HAS_BUFFEREVENT(conn, {
        do bufferevent stuff;
      }) ELSE_IF_NO_BUFFEREVENT {
        do non-bufferevent stuff;
      }
   If we're compiling with bufferevent support, then the macros expand more or
   less to:
      if (conn->bufev) {
        do_bufferevent_stuff;
      } else {
        do non-bufferevent stuff;
      }
   and if we aren't using bufferevents, they expand more or less to:
      { do non-bufferevent stuff; }
*/
#ifdef USE_BUFFEREVENTS
#define HAS_BUFFEREVENT(c) (((c)->bufev) != NULL)
#define IF_HAS_BUFFEREVENT(c, stmt)                \
  if ((c)->bufev) do {                             \
      stmt ;                                       \
  } while (0)
#define ELSE_IF_NO_BUFFEREVENT ; else
#define IF_HAS_NO_BUFFEREVENT(c)                   \
  if (NULL == (c)->bufev)
#else
#define HAS_BUFFEREVENT(c) (0)
#define IF_HAS_BUFFEREVENT(c, stmt) (void)0
#define ELSE_IF_NO_BUFFEREVENT ;
#define IF_HAS_NO_BUFFEREVENT(c)                \
  if (1)
#endif

/** What action type does an address policy indicate: accept or reject? */
typedef enum {
  ADDR_POLICY_ACCEPT=1,
  ADDR_POLICY_REJECT=2,
} addr_policy_action_t;
#define addr_policy_action_bitfield_t ENUM_BF(addr_policy_action_t)

/** A reference-counted address policy rule. */
typedef struct addr_policy_t {
  int refcnt; /**< Reference count */
  /** What to do when the policy matches.*/
  addr_policy_action_bitfield_t policy_type:2;
  unsigned int is_private:1; /**< True iff this is the pseudo-address,
                              * "private". */
  unsigned int is_canonical:1; /**< True iff this policy is the canonical
                                * copy (stored in a hash table to avoid
                                * duplication of common policies) */
  maskbits_t maskbits; /**< Accept/reject all addresses <b>a</b> such that the
                 * first <b>maskbits</b> bits of <b>a</b> match
                 * <b>addr</b>. */
  /** Base address to accept or reject.
   *
   * Note that wildcards are treated
   * differntly depending on address family. An AF_UNSPEC address means
   * "All addresses, IPv4 or IPv6." An AF_INET address with maskbits==0 means
   * "All IPv4 addresses" and an AF_INET6 address with maskbits == 0 means
   * "All IPv6 addresses".
  **/
  tor_addr_t addr;
  uint16_t prt_min; /**< Lowest port number to accept/reject. */
  uint16_t prt_max; /**< Highest port number to accept/reject. */
} addr_policy_t;

/** A cached_dir_t represents a cacheable directory object, along with its
 * compressed form. */
typedef struct cached_dir_t {
  char *dir; /**< Contents of this object, NUL-terminated. */
  char *dir_z; /**< Compressed contents of this object. */
  size_t dir_len; /**< Length of <b>dir</b> (not counting its NUL). */
  size_t dir_z_len; /**< Length of <b>dir_z</b>. */
  time_t published; /**< When was this object published. */
  digests_t digests; /**< Digests of this object (networkstatus only) */
  int refcnt; /**< Reference count for this cached_dir_t. */
} cached_dir_t;

/** Enum used to remember where a signed_descriptor_t is stored and how to
 * manage the memory for signed_descriptor_body.  */
typedef enum {
  /** The descriptor isn't stored on disk at all: the copy in memory is
   * canonical; the saved_offset field is meaningless. */
  SAVED_NOWHERE=0,
  /** The descriptor is stored in the cached_routers file: the
   * signed_descriptor_body is meaningless; the signed_descriptor_len and
   * saved_offset are used to index into the mmaped cache file. */
  SAVED_IN_CACHE,
  /** The descriptor is stored in the cached_routers.new file: the
   * signed_descriptor_body and saved_offset fields are both set. */
  /* FFFF (We could also mmap the file and grow the mmap as needed, or
   * lazy-load the descriptor text by using seek and read.  We don't, for
   * now.)
   */
  SAVED_IN_JOURNAL
} saved_location_t;
#define saved_location_bitfield_t ENUM_BF(saved_location_t)

/** Enumeration: what kind of download schedule are we using for a given
 * object? */
typedef enum {
  DL_SCHED_GENERIC = 0,
  DL_SCHED_CONSENSUS = 1,
  DL_SCHED_BRIDGE = 2,
} download_schedule_t;
#define download_schedule_bitfield_t ENUM_BF(download_schedule_t)

/** Information about our plans for retrying downloads for a downloadable
 * object. */
typedef struct download_status_t {
  time_t next_attempt_at; /**< When should we try downloading this descriptor
                           * again? */
  uint8_t n_download_failures; /**< Number of failures trying to download the
                                * most recent descriptor. */
  download_schedule_bitfield_t schedule : 8;
} download_status_t;

/** If n_download_failures is this high, the download can never happen. */
#define IMPOSSIBLE_TO_DOWNLOAD 255

/** The max size we expect router descriptor annotations we create to
 * be. We'll accept larger ones if we see them on disk, but we won't
 * create any that are larger than this. */
#define ROUTER_ANNOTATION_BUF_LEN 256

/** Information need to cache an onion router's descriptor. */
typedef struct signed_descriptor_t {
  /** Pointer to the raw server descriptor, preceded by annotations.  Not
   * necessarily NUL-terminated.  If saved_location is SAVED_IN_CACHE, this
   * pointer is null. */
  char *signed_descriptor_body;
  /** Length of the annotations preceding the server descriptor. */
  size_t annotations_len;
  /** Length of the server descriptor. */
  size_t signed_descriptor_len;
  /** Digest of the server descriptor, computed as specified in
   * dir-spec.txt. */
  char signed_descriptor_digest[DIGEST_LEN];
  /** Identity digest of the router. */
  char identity_digest[DIGEST_LEN];
  /** Declared publication time of the descriptor. */
  time_t published_on;
  /** For routerdescs only: digest of the corresponding extrainfo. */
  char extra_info_digest[DIGEST_LEN];
  /** For routerdescs only: Status of downloading the corresponding
   * extrainfo. */
  download_status_t ei_dl_status;
  /** Where is the descriptor saved? */
  saved_location_t saved_location;
  /** If saved_location is SAVED_IN_CACHE or SAVED_IN_JOURNAL, the offset of
   * this descriptor in the corresponding file. */
  off_t saved_offset;
  /** What position is this descriptor within routerlist->routers or
   * routerlist->old_routers? -1 for none. */
  int routerlist_index;
  /** The valid-until time of the most recent consensus that listed this
   * descriptor.  0 for "never listed in a consensus, so far as we know." */
  time_t last_listed_as_valid_until;
  /* If true, we do not ever try to save this object in the cache. */
  unsigned int do_not_cache : 1;
  /* If true, this item is meant to represent an extrainfo. */
  unsigned int is_extrainfo : 1;
  /* If true, we got an extrainfo for this item, and the digest was right,
   * but it was incompatible. */
  unsigned int extrainfo_is_bogus : 1;
  /* If true, we are willing to transmit this item unencrypted. */
  unsigned int send_unencrypted : 1;
} signed_descriptor_t;

/** A signed integer representing a country code. */
typedef int16_t country_t;

/** Information about another onion router in the network. */
typedef struct {
  signed_descriptor_t cache_info;
  char *nickname; /**< Human-readable OR name. */

  uint32_t addr; /**< IPv4 address of OR, in host order. */
  uint16_t or_port; /**< Port for TLS connections. */
  uint16_t dir_port; /**< Port for HTTP directory connections. */

  /** A router's IPv6 address, if it has one. */
  /* XXXXX187 Actually these should probably be part of a list of addresses,
   * not just a special case.  Use abstractions to access these; don't do it
   * directly. */
  tor_addr_t ipv6_addr;
  uint16_t ipv6_orport;

  crypto_pk_t *onion_pkey; /**< Public RSA key for onions. */
  crypto_pk_t *identity_pkey;  /**< Public RSA key for signing. */
  /** Public curve25519 key for onions */
  curve25519_public_key_t *onion_curve25519_pkey;

  char *platform; /**< What software/operating system is this OR using? */

  /* link info */
  uint32_t bandwidthrate; /**< How many bytes does this OR add to its token
                           * bucket per second? */
  uint32_t bandwidthburst; /**< How large is this OR's token bucket? */
  /** How many bytes/s is this router known to handle? */
  uint32_t bandwidthcapacity;
  smartlist_t *exit_policy; /**< What streams will this OR permit
                             * to exit on IPv4?  NULL for 'reject *:*'. */
  /** What streams will this OR permit to exit on IPv6?
   * NULL for 'reject *:*' */
  struct short_policy_t *ipv6_exit_policy;
  long uptime; /**< How many seconds the router claims to have been up */
  smartlist_t *declared_family; /**< Nicknames of router which this router
                                 * claims are its family. */
  char *contact_info; /**< Declared contact info for this router. */
  unsigned int is_hibernating:1; /**< Whether the router claims to be
                                  * hibernating */
  unsigned int caches_extra_info:1; /**< Whether the router says it caches and
                                     * serves extrainfo documents. */
  unsigned int allow_single_hop_exits:1;  /**< Whether the router says
                                           * it allows single hop exits. */

  unsigned int wants_to_be_hs_dir:1; /**< True iff this router claims to be
                                      * a hidden service directory. */
  unsigned int policy_is_reject_star:1; /**< True iff the exit policy for this
                                         * router rejects everything. */
  /** True if, after we have added this router, we should re-launch
   * tests for it. */
  unsigned int needs_retest_if_added:1;

/** Tor can use this router for general positions in circuits; we got it
 * from a directory server as usual, or we're an authority and a server
 * uploaded it. */
#define ROUTER_PURPOSE_GENERAL 0
/** Tor should avoid using this router for circuit-building: we got it
 * from a crontroller.  If the controller wants to use it, it'll have to
 * ask for it by identity. */
#define ROUTER_PURPOSE_CONTROLLER 1
/** Tor should use this router only for bridge positions in circuits: we got
 * it via a directory request from the bridge itself, or a bridge
 * authority. x*/
#define ROUTER_PURPOSE_BRIDGE 2
/** Tor should not use this router; it was marked in cached-descriptors with
 * a purpose we didn't recognize. */
#define ROUTER_PURPOSE_UNKNOWN 255

  /* In what way did we find out about this router?  One of ROUTER_PURPOSE_*.
   * Routers of different purposes are kept segregated and used for different
   * things; see notes on ROUTER_PURPOSE_* macros above.
   */
  uint8_t purpose;
} routerinfo_t;

/** Information needed to keep and cache a signed extra-info document. */
typedef struct extrainfo_t {
  signed_descriptor_t cache_info;
  /** The router's nickname. */
  char nickname[MAX_NICKNAME_LEN+1];
  /** True iff we found the right key for this extra-info, verified the
   * signature, and found it to be bad. */
  unsigned int bad_sig : 1;
  /** If present, we didn't have the right key to verify this extra-info,
   * so this is a copy of the signature in the document. */
  char *pending_sig;
  /** Length of pending_sig. */
  size_t pending_sig_len;
} extrainfo_t;

/** Contents of a single router entry in a network status object.
 */
typedef struct routerstatus_t {
  time_t published_on; /**< When was this router published? */
  char nickname[MAX_NICKNAME_LEN+1]; /**< The nickname this router says it
                                      * has. */
  char identity_digest[DIGEST_LEN]; /**< Digest of the router's identity
                                     * key. */
  /** Digest of the router's most recent descriptor or microdescriptor.
   * If it's a descriptor, we only use the first DIGEST_LEN bytes. */
  char descriptor_digest[DIGEST256_LEN];
  uint32_t addr; /**< IPv4 address for this router. */
  uint16_t or_port; /**< OR port for this router. */
  uint16_t dir_port; /**< Directory port for this router. */
  tor_addr_t ipv6_addr; /**< IPv6 address for this router. */
  uint16_t ipv6_orport; /**<IPV6 OR port for this router. */
  unsigned int is_authority:1; /**< True iff this router is an authority. */
  unsigned int is_exit:1; /**< True iff this router is a good exit. */
  unsigned int is_stable:1; /**< True iff this router stays up a long time. */
  unsigned int is_fast:1; /**< True iff this router has good bandwidth. */
  /** True iff this router is called 'running' in the consensus. We give it
   * this funny name so that we don't accidentally use this bit as a view of
   * whether we think the router is *currently* running.  If that's what you
   * want to know, look at is_running in node_t. */
  unsigned int is_flagged_running:1;
  unsigned int is_named:1; /**< True iff "nickname" belongs to this router. */
  unsigned int is_unnamed:1; /**< True iff "nickname" belongs to another
                              * router. */
  unsigned int is_valid:1; /**< True iff this router isn't invalid. */
  unsigned int is_possible_guard:1; /**< True iff this router would be a good
                                     * choice as an entry guard. */
  unsigned int is_bad_exit:1; /**< True iff this node is a bad choice for
                               * an exit node. */
  unsigned int is_bad_directory:1; /**< Do we think this directory is junky,
                                    * underpowered, or otherwise useless? */
  unsigned int is_hs_dir:1; /**< True iff this router is a v2-or-later hidden
                             * service directory. */
  /** True iff we know version info for this router. (i.e., a "v" entry was
   * included.)  We'll replace all these with a big tor_version_t or a char[]
   * if the number of traits we care about ever becomes incredibly big. */
  unsigned int version_known:1;

  /** True iff this router is a version that, if it caches directory info,
   * we can get microdescriptors from. */
  unsigned int version_supports_microdesc_cache:1;
  /** True iff this router is a version that allows DATA cells to arrive on
   * a stream before it has sent a CONNECTED cell. */
  unsigned int version_supports_optimistic_data:1;
  /** True iff this router has a version that allows it to accept EXTEND2
   * cells */
  unsigned int version_supports_extend2_cells:1;

  unsigned int has_bandwidth:1; /**< The vote/consensus had bw info */
  unsigned int has_exitsummary:1; /**< The vote/consensus had exit summaries */
  unsigned int bw_is_unmeasured:1; /**< This is a consensus entry, with
                                    * the Unmeasured flag set. */

  uint32_t bandwidth_kb; /**< Bandwidth (capacity) of the router as reported in
                       * the vote/consensus, in kilobytes/sec. */
  char *exitsummary; /**< exit policy summary -
                      * XXX weasel: this probably should not stay a string. */

  /* ---- The fields below aren't derived from the networkstatus; they
   * hold local information only. */

  time_t last_dir_503_at; /**< When did this router last tell us that it
                           * was too busy to serve directory info? */
  download_status_t dl_status;

} routerstatus_t;

/** A single entry in a parsed policy summary, describing a range of ports. */
typedef struct short_policy_entry_t {
  uint16_t min_port, max_port;
} short_policy_entry_t;

/** A short_poliy_t is the parsed version of a policy summary. */
typedef struct short_policy_t {
  /** True if the members of 'entries' are port ranges to accept; false if
   * they are port ranges to reject */
  unsigned int is_accept : 1;
  /** The actual number of values in 'entries'. */
  unsigned int n_entries : 31;
  /** An array of 0 or more short_policy_entry_t values, each describing a
   * range of ports that this policy accepts or rejects (depending on the
   * value of is_accept).
   */
  short_policy_entry_t entries[FLEXIBLE_ARRAY_MEMBER];
} short_policy_t;

/** A microdescriptor is the smallest amount of information needed to build a
 * circuit through a router.  They are generated by the directory authorities,
 * using information from the uploaded routerinfo documents.  They are not
 * self-signed, but are rather authenticated by having their hash in a signed
 * networkstatus document. */
typedef struct microdesc_t {
  /** Hashtable node, used to look up the microdesc by its digest. */
  HT_ENTRY(microdesc_t) node;

  /* Cache information */

  /**  When was this microdescriptor last listed in a consensus document?
   * Once a microdesc has been unlisted long enough, we can drop it.
   */
  time_t last_listed;
  /** Where is this microdescriptor currently stored? */
  saved_location_bitfield_t saved_location : 3;
  /** If true, do not attempt to cache this microdescriptor on disk. */
  unsigned int no_save : 1;
  /** If true, this microdesc has an entry in the microdesc_map */
  unsigned int held_in_map : 1;
  /** Reference count: how many node_ts have a reference to this microdesc? */
  unsigned int held_by_nodes;

  /** If saved_location == SAVED_IN_CACHE, this field holds the offset of the
   * microdescriptor in the cache. */
  off_t off;

  /* The string containing the microdesc. */

  /** A pointer to the encoded body of the microdescriptor.  If the
   * saved_location is SAVED_IN_CACHE, then the body is a pointer into an
   * mmap'd region.  Otherwise, it is a malloc'd string.  The string might not
   * be NUL-terminated; take the length from <b>bodylen</b>. */
  char *body;
  /** The length of the microdescriptor in <b>body</b>. */
  size_t bodylen;
  /** A SHA256-digest of the microdescriptor. */
  char digest[DIGEST256_LEN];

  /* Fields in the microdescriptor. */

  /** As routerinfo_t.onion_pkey */
  crypto_pk_t *onion_pkey;
  /** As routerinfo_t.onion_curve25519_pkey */
  curve25519_public_key_t *onion_curve25519_pkey;
  /** As routerinfo_t.ipv6_add */
  tor_addr_t ipv6_addr;
  /** As routerinfo_t.ipv6_orport */
  uint16_t ipv6_orport;
  /** As routerinfo_t.family */
  smartlist_t *family;
  /** IPv4 exit policy summary */
  short_policy_t *exit_policy;
  /** IPv6 exit policy summary */
  short_policy_t *ipv6_exit_policy;

} microdesc_t;

/** A node_t represents a Tor router.
 *
 * Specifically, a node_t is a Tor router as we are using it: a router that
 * we are considering for circuits, connections, and so on.  A node_t is a
 * thin wrapper around the routerstatus, routerinfo, and microdesc for a
 * single wrapper, and provides a consistent interface for all of them.
 *
 * Also, a node_t has mutable state.  While a routerinfo, a routerstatus,
 * and a microdesc have[*] only the information read from a router
 * descriptor, a consensus entry, and a microdescriptor (respectively)...
 * a node_t has flags based on *our own current opinion* of the node.
 *
 * [*] Actually, there is some leftover information in each that is mutable.
 *  We should try to excise that.
 */
typedef struct node_t {
  /* Indexing information */

  /** Used to look up the node_t by its identity digest. */
  HT_ENTRY(node_t) ht_ent;
  /** Position of the node within the list of nodes */
  int nodelist_idx;

  /** The identity digest of this node_t.  No more than one node_t per
   * identity may exist at a time. */
  char identity[DIGEST_LEN];

  microdesc_t *md;
  routerinfo_t *ri;
  routerstatus_t *rs;

  /* local info: copied from routerstatus, then possibly frobbed based
   * on experience.  Authorities set this stuff directly.  Note that
   * these reflect knowledge of the primary (IPv4) OR port only.  */

  unsigned int is_running:1; /**< As far as we know, is this OR currently
                              * running? */
  unsigned int is_valid:1; /**< Has a trusted dirserver validated this OR?
                            *  (For Authdir: Have we validated this OR?) */
  unsigned int is_fast:1; /** Do we think this is a fast OR? */
  unsigned int is_stable:1; /** Do we think this is a stable OR? */
  unsigned int is_possible_guard:1; /**< Do we think this is an OK guard? */
  unsigned int is_exit:1; /**< Do we think this is an OK exit? */
  unsigned int is_bad_exit:1; /**< Do we think this exit is censored, borked,
                               * or otherwise nasty? */
  unsigned int is_bad_directory:1; /**< Do we think this directory is junky,
                                    * underpowered, or otherwise useless? */
  unsigned int is_hs_dir:1; /**< True iff this router is a hidden service
                             * directory according to the authorities. */

  /* Local info: warning state. */

  unsigned int name_lookup_warned:1; /**< Have we warned the user for referring
                                      * to this (unnamed) router by nickname?
                                      */

  /** Local info: we treat this node as if it rejects everything */
  unsigned int rejects_all:1;

  /** Local info: this node is in our list of guards */
  unsigned int using_as_guard:1;

  /* Local info: derived. */

  /** True if the IPv6 OR port is preferred over the IPv4 OR port.  */
  unsigned int ipv6_preferred:1;

  /** According to the geoip db what country is this router in? */
  /* XXXprop186 what is this suppose to mean with multiple OR ports? */
  country_t country;

  /* The below items are used only by authdirservers for
   * reachability testing. */

  /** When was the last time we could reach this OR? */
  time_t last_reachable;        /* IPv4. */
  time_t last_reachable6;       /* IPv6. */

} node_t;

/** Linked list of microdesc hash lines for a single router in a directory
 * vote.
 */
typedef struct vote_microdesc_hash_t {
  /** Next element in the list, or NULL. */
  struct vote_microdesc_hash_t *next;
  /** The raw contents of the microdesc hash line, from the "m" through the
   * newline. */
  char *microdesc_hash_line;
} vote_microdesc_hash_t;

/** The claim about a single router, made in a vote. */
typedef struct vote_routerstatus_t {
  routerstatus_t status; /**< Underlying 'status' object for this router.
                          * Flags are redundant. */
  /** How many known-flags are allowed in a vote? This is the width of
   * the flags field of vote_routerstatus_t */
#define MAX_KNOWN_FLAGS_IN_VOTE 64
  uint64_t flags; /**< Bit-field for all recognized flags; index into
                   * networkstatus_t.known_flags. */
  char *version; /**< The version that the authority says this router is
                  * running. */
  unsigned int has_measured_bw:1; /**< The vote had a measured bw */
  uint32_t measured_bw_kb; /**< Measured bandwidth (capacity) of the router */
  /** The hash or hashes that the authority claims this microdesc has. */
  vote_microdesc_hash_t *microdesc;
} vote_routerstatus_t;

/** A signature of some document by an authority. */
typedef struct document_signature_t {
  /** Declared SHA-1 digest of this voter's identity key */
  char identity_digest[DIGEST_LEN];
  /** Declared SHA-1 digest of signing key used by this voter. */
  char signing_key_digest[DIGEST_LEN];
  /** Algorithm used to compute the digest of the document. */
  digest_algorithm_t alg;
  /** Signature of the signed thing. */
  char *signature;
  /** Length of <b>signature</b> */
  int signature_len;
  unsigned int bad_signature : 1; /**< Set to true if we've tried to verify
                                   * the sig, and we know it's bad. */
  unsigned int good_signature : 1; /**< Set to true if we've verified the sig
                                     * as good. */
} document_signature_t;

/** Information about a single voter in a vote or a consensus. */
typedef struct networkstatus_voter_info_t {
  /** Declared SHA-1 digest of this voter's identity key */
  char identity_digest[DIGEST_LEN];
  char *nickname; /**< Nickname of this voter */
  /** Digest of this voter's "legacy" identity key, if any.  In vote only; for
   * consensuses, we treat legacy keys as additional signers. */
  char legacy_id_digest[DIGEST_LEN];
  char *address; /**< Address of this voter, in string format. */
  uint32_t addr; /**< Address of this voter, in IPv4, in host order. */
  uint16_t dir_port; /**< Directory port of this voter */
  uint16_t or_port; /**< OR port of this voter */
  char *contact; /**< Contact information for this voter. */
  char vote_digest[DIGEST_LEN]; /**< Digest of this voter's vote, as signed. */

  /* Nothing from here on is signed. */
  /** The signature of the document and the signature's status. */
  smartlist_t *sigs;
} networkstatus_voter_info_t;

/** Enumerates the possible seriousness values of a networkstatus document. */
typedef enum {
  NS_TYPE_VOTE,
  NS_TYPE_CONSENSUS,
  NS_TYPE_OPINION,
} networkstatus_type_t;

/** Enumerates recognized flavors of a consensus networkstatus document.  All
 * flavors of a consensus are generated from the same set of votes, but they
 * present different types information to different versions of Tor. */
typedef enum {
  FLAV_NS = 0,
  FLAV_MICRODESC = 1,
} consensus_flavor_t;

/** How many different consensus flavors are there? */
#define N_CONSENSUS_FLAVORS ((int)(FLAV_MICRODESC)+1)

/** A common structure to hold a v3 network status vote, or a v3 network
 * status consensus. */
typedef struct networkstatus_t {
  networkstatus_type_t type; /**< Vote, consensus, or opinion? */
  consensus_flavor_t flavor; /**< If a consensus, what kind? */
  unsigned int has_measured_bws : 1;/**< True iff this networkstatus contains
                                     * measured= bandwidth values. */

  time_t published; /**< Vote only: Time when vote was written. */
  time_t valid_after; /**< Time after which this vote or consensus applies. */
  time_t fresh_until; /**< Time before which this is the most recent vote or
                       * consensus. */
  time_t valid_until; /**< Time after which this vote or consensus should not
                       * be used. */

  /** Consensus only: what method was used to produce this consensus? */
  int consensus_method;
  /** Vote only: what methods is this voter willing to use? */
  smartlist_t *supported_methods;

  /** How long does this vote/consensus claim that authorities take to
   * distribute their votes to one another? */
  int vote_seconds;
  /** How long does this vote/consensus claim that authorities take to
   * distribute their consensus signatures to one another? */
  int dist_seconds;

  /** Comma-separated list of recommended client software, or NULL if this
   * voter has no opinion. */
  char *client_versions;
  char *server_versions;
  /** List of flags that this vote/consensus applies to routers.  If a flag is
   * not listed here, the voter has no opinion on what its value should be. */
  smartlist_t *known_flags;

  /** List of key=value strings for the parameters in this vote or
   * consensus, sorted by key. */
  smartlist_t *net_params;

  /** List of key=value strings for the bw weight parameters in the
   * consensus. */
  smartlist_t *weight_params;

  /** List of networkstatus_voter_info_t.  For a vote, only one element
   * is included.  For a consensus, one element is included for every voter
   * whose vote contributed to the consensus. */
  smartlist_t *voters;

  struct authority_cert_t *cert; /**< Vote only: the voter's certificate. */

  /** Digests of this document, as signed. */
  digests_t digests;

  /** List of router statuses, sorted by identity digest.  For a vote,
   * the elements are vote_routerstatus_t; for a consensus, the elements
   * are routerstatus_t. */
  smartlist_t *routerstatus_list;

  /** If present, a map from descriptor digest to elements of
   * routerstatus_list. */
  digestmap_t *desc_digest_map;
} networkstatus_t;

/** A set of signatures for a networkstatus consensus.  Unless otherwise
 * noted, all fields are as for networkstatus_t. */
typedef struct ns_detached_signatures_t {
  time_t valid_after;
  time_t fresh_until;
  time_t valid_until;
  strmap_t *digests; /**< Map from flavor name to digestset_t */
  strmap_t *signatures; /**< Map from flavor name to list of
                         * document_signature_t */
} ns_detached_signatures_t;

/** Allowable types of desc_store_t. */
typedef enum store_type_t {
  ROUTER_STORE = 0,
  EXTRAINFO_STORE = 1
} store_type_t;

/** A 'store' is a set of descriptors saved on disk, with accompanying
 * journal, mmaped as needed, rebuilt as needed. */
typedef struct desc_store_t {
  /** Filename (within DataDir) for the store.  We append .tmp to this
   * filename for a temporary file when rebuilding the store, and .new to this
   * filename for the journal. */
  const char *fname_base;
  /** Human-readable description of what this store contains. */
  const char *description;

  tor_mmap_t *mmap; /**< A mmap for the main file in the store. */

  store_type_t type; /**< What's stored in this store? */

  /** The size of the router log, in bytes. */
  size_t journal_len;
  /** The size of the router store, in bytes. */
  size_t store_len;
  /** Total bytes dropped since last rebuild: this is space currently
   * used in the cache and the journal that could be freed by a rebuild. */
  size_t bytes_dropped;
} desc_store_t;

/** Contents of a directory of onion routers. */
typedef struct {
  /** Map from server identity digest to a member of routers. */
  struct digest_ri_map_t *identity_map;
  /** Map from server descriptor digest to a signed_descriptor_t from
   * routers or old_routers. */
  struct digest_sd_map_t *desc_digest_map;
  /** Map from extra-info digest to an extrainfo_t.  Only exists for
   * routers in routers or old_routers. */
  struct digest_ei_map_t *extra_info_map;
  /** Map from extra-info digests to a signed_descriptor_t for a router
   * descriptor having that extra-info digest.  Only exists for
   * routers in routers or old_routers. */
  struct digest_sd_map_t *desc_by_eid_map;
  /** List of routerinfo_t for all currently live routers we know. */
  smartlist_t *routers;
  /** List of signed_descriptor_t for older router descriptors we're
   * caching. */
  smartlist_t *old_routers;
  /** Store holding server descriptors.  If present, any router whose
   * cache_info.saved_location == SAVED_IN_CACHE is stored in this file
   * starting at cache_info.saved_offset */
  desc_store_t desc_store;
  /** Store holding extra-info documents. */
  desc_store_t extrainfo_store;
} routerlist_t;

/** Information on router used when extending a circuit. We don't need a
 * full routerinfo_t to extend: we only need addr:port:keyid to build an OR
 * connection, and onion_key to create the onionskin. Note that for onehop
 * general-purpose tunnels, the onion_key is NULL. */
typedef struct extend_info_t {
  char nickname[MAX_HEX_NICKNAME_LEN+1]; /**< This router's nickname for
                                          * display. */
  char identity_digest[DIGEST_LEN]; /**< Hash of this router's identity key. */
  uint16_t port; /**< OR port. */
  tor_addr_t addr; /**< IP address. */
  crypto_pk_t *onion_key; /**< Current onionskin key. */
#ifdef CURVE25519_ENABLED
  curve25519_public_key_t curve25519_onion_key;
#endif
} extend_info_t;

/** Certificate for v3 directory protocol: binds long-term authority identity
 * keys to medium-term authority signing keys. */
typedef struct authority_cert_t {
  /** Information relating to caching this cert on disk and looking it up. */
  signed_descriptor_t cache_info;
  /** This authority's long-term authority identity key. */
  crypto_pk_t *identity_key;
  /** This authority's medium-term signing key. */
  crypto_pk_t *signing_key;
  /** The digest of <b>signing_key</b> */
  char signing_key_digest[DIGEST_LEN];
  /** The listed expiration time of this certificate. */
  time_t expires;
  /** This authority's IPv4 address, in host order. */
  uint32_t addr;
  /** This authority's directory port. */
  uint16_t dir_port;
} authority_cert_t;

/** Bitfield enum type listing types of information that directory authorities
 * can be authoritative about, and that directory caches may or may not cache.
 *
 * Note that the granularity here is based on authority granularity and on
 * cache capabilities.  Thus, one particular bit may correspond in practice to
 * a few types of directory info, so long as every authority that pronounces
 * officially about one of the types prounounces officially about all of them,
 * and so long as every cache that caches one of them caches all of them.
 */
typedef enum {
  NO_DIRINFO      = 0,
  /** Serves/signs v3 directory information: votes, consensuses, certs */
  V3_DIRINFO      = 1 << 2,
  /** Serves bridge descriptors. */
  BRIDGE_DIRINFO  = 1 << 4,
  /** Serves extrainfo documents. */
  EXTRAINFO_DIRINFO=1 << 5,
  /** Serves microdescriptors. */
  MICRODESC_DIRINFO=1 << 6,
} dirinfo_type_t;

#define ALL_DIRINFO ((dirinfo_type_t)((1<<7)-1))

#define CRYPT_PATH_MAGIC 0x70127012u

struct fast_handshake_state_t;
struct ntor_handshake_state_t;
#define ONION_HANDSHAKE_TYPE_TAP  0x0000
#define ONION_HANDSHAKE_TYPE_FAST 0x0001
#define ONION_HANDSHAKE_TYPE_NTOR 0x0002
#define MAX_ONION_HANDSHAKE_TYPE 0x0002
typedef struct {
  uint16_t tag;
  union {
    struct fast_handshake_state_t *fast;
    crypto_dh_t *tap;
    struct ntor_handshake_state_t *ntor;
  } u;
} onion_handshake_state_t;

/** Holds accounting information for a single step in the layered encryption
 * performed by a circuit.  Used only at the client edge of a circuit. */
typedef struct crypt_path_t {
  uint32_t magic;

  /* crypto environments */
  /** Encryption key and counter for cells heading towards the OR at this
   * step. */
  crypto_cipher_t *f_crypto;
  /** Encryption key and counter for cells heading back from the OR at this
   * step. */
  crypto_cipher_t *b_crypto;

  /** Digest state for cells heading towards the OR at this step. */
  crypto_digest_t *f_digest; /* for integrity checking */
  /** Digest state for cells heading away from the OR at this step. */
  crypto_digest_t *b_digest;

  /** Current state of the handshake as performed with the OR at this
   * step. */
  onion_handshake_state_t handshake_state;
  /** Diffie-hellman handshake state for performing an introduction
   * operations */
  crypto_dh_t *rend_dh_handshake_state;

  /** Negotiated key material shared with the OR at this step. */
  char rend_circ_nonce[DIGEST_LEN];/* KH in tor-spec.txt */

  /** Information to extend to the OR at this step. */
  extend_info_t *extend_info;

  /** Is the circuit built to this step?  Must be one of:
   *    - CPATH_STATE_CLOSED (The circuit has not been extended to this step)
   *    - CPATH_STATE_AWAITING_KEYS (We have sent an EXTEND/CREATE to this step
   *      and not received an EXTENDED/CREATED)
   *    - CPATH_STATE_OPEN (The circuit has been extended to this step) */
  uint8_t state;
#define CPATH_STATE_CLOSED 0
#define CPATH_STATE_AWAITING_KEYS 1
#define CPATH_STATE_OPEN 2
  struct crypt_path_t *next; /**< Link to next crypt_path_t in the circuit.
                              * (The list is circular, so the last node
                              * links to the first.) */
  struct crypt_path_t *prev; /**< Link to previous crypt_path_t in the
                              * circuit. */

  int package_window; /**< How many cells are we allowed to originate ending
                       * at this step? */
  int deliver_window; /**< How many cells are we willing to deliver originating
                       * at this step? */
} crypt_path_t;

/** A reference-counted pointer to a crypt_path_t, used only to share
 * the final rendezvous cpath to be used on a service-side rendezvous
 * circuit among multiple circuits built in parallel to the same
 * destination rendezvous point. */
typedef struct {
  /** The reference count. */
  unsigned int refcount;
  /** The pointer.  Set to NULL when the crypt_path_t is put into use
   * on an opened rendezvous circuit. */
  crypt_path_t *cpath;
} crypt_path_reference_t;

#define CPATH_KEY_MATERIAL_LEN (20*2+16*2)

#define DH_KEY_LEN DH_BYTES

/** Information used to build a circuit. */
typedef struct {
  /** Intended length of the final circuit. */
  int desired_path_len;
  /** How to extend to the planned exit node. */
  extend_info_t *chosen_exit;
  /** Whether every node in the circ must have adequate uptime. */
  unsigned int need_uptime : 1;
  /** Whether every node in the circ must have adequate capacity. */
  unsigned int need_capacity : 1;
  /** Whether the last hop was picked with exiting in mind. */
  unsigned int is_internal : 1;
  /** Did we pick this as a one-hop tunnel (not safe for other streams)?
   * These are for encrypted dir conns that exit to this router, not
   * for arbitrary exits from the circuit. */
  unsigned int onehop_tunnel : 1;
  /** The crypt_path_t to append after rendezvous: used for rendezvous. */
  crypt_path_t *pending_final_cpath;
  /** A ref-counted reference to the crypt_path_t to append after
   * rendezvous; used on the service side. */
  crypt_path_reference_t *service_pending_final_cpath_ref;
  /** How many times has building a circuit for this task failed? */
  int failure_count;
  /** At what time should we give up on this task? */
  time_t expiry_time;
} cpath_build_state_t;

#define ORIGIN_CIRCUIT_MAGIC 0x35315243u
#define OR_CIRCUIT_MAGIC 0x98ABC04Fu

struct create_cell_t;

/** Entry in the cell stats list of a circuit; used only if CELL_STATS
 * events are enabled. */
typedef struct testing_cell_stats_entry_t {
  uint8_t command; /**< cell command number. */
  /** Waiting time in centiseconds if this event is for a removed cell,
   * or 0 if this event is for adding a cell to the queue.  22 bits can
   * store more than 11 hours, enough to assume that a circuit with this
   * delay would long have been closed. */
  unsigned int waiting_time:22;
  unsigned int removed:1; /**< 0 for added to, 1 for removed from queue. */
  unsigned int exitward:1; /**< 0 for app-ward, 1 for exit-ward. */
} testing_cell_stats_entry_t;

/**
 * A circuit is a path over the onion routing
 * network. Applications can connect to one end of the circuit, and can
 * create exit connections at the other end of the circuit. AP and exit
 * connections have only one circuit associated with them (and thus these
 * connection types are closed when the circuit is closed), whereas
 * OR connections multiplex many circuits at once, and stay standing even
 * when there are no circuits running over them.
 *
 * A circuit_t structure can fill one of two roles.  First, a or_circuit_t
 * links two connections together: either an edge connection and an OR
 * connection, or two OR connections.  (When joined to an OR connection, a
 * circuit_t affects only cells sent to a particular circID on that
 * connection.  When joined to an edge connection, a circuit_t affects all
 * data.)

 * Second, an origin_circuit_t holds the cipher keys and state for sending data
 * along a given circuit.  At the OP, it has a sequence of ciphers, each
 * of which is shared with a single OR along the circuit.  Separate
 * ciphers are used for data going "forward" (away from the OP) and
 * "backward" (towards the OP).  At the OR, a circuit has only two stream
 * ciphers: one for data going forward, and one for data going backward.
 */
typedef struct circuit_t {
  uint32_t magic; /**< For memory and type debugging: must equal
                   * ORIGIN_CIRCUIT_MAGIC or OR_CIRCUIT_MAGIC. */

  /** The channel that is next in this circuit. */
  channel_t *n_chan;

  /**
   * The circuit_id used in the next (forward) hop of this circuit;
   * this is unique to n_chan, but this ordered pair is globally
   * unique:
   *
   * (n_chan->global_identifier, n_circ_id)
   */
  circid_t n_circ_id;

  /**
   * Circuit mux associated with n_chan to which this circuit is attached;
   * NULL if we have no n_chan.
   */
  circuitmux_t *n_mux;

  /** Queue of cells waiting to be transmitted on n_chan */
  cell_queue_t n_chan_cells;

  /**
   * The hop to which we want to extend this circuit.  Should be NULL if
   * the circuit has attached to a channel.
   */
  extend_info_t *n_hop;

  /** True iff we are waiting for n_chan_cells to become less full before
   * allowing p_streams to add any more cells. (Origin circuit only.) */
  unsigned int streams_blocked_on_n_chan : 1;
  /** True iff we are waiting for p_chan_cells to become less full before
   * allowing n_streams to add any more cells. (OR circuit only.) */
  unsigned int streams_blocked_on_p_chan : 1;

  /** True iff we have queued a delete backwards on this circuit, but not put
   * it on the output buffer. */
  unsigned int p_delete_pending : 1;
  /** True iff we have queued a delete forwards on this circuit, but not put
   * it on the output buffer. */
  unsigned int n_delete_pending : 1;

  /** True iff this circuit has received a DESTROY cell in either direction */
  unsigned int received_destroy : 1;

  uint8_t state; /**< Current status of this circuit. */
  uint8_t purpose; /**< Why are we creating this circuit? */

  /** How many relay data cells can we package (read from edge streams)
   * on this circuit before we receive a circuit-level sendme cell asking
   * for more? */
  int package_window;
  /** How many relay data cells will we deliver (write to edge streams)
   * on this circuit? When deliver_window gets low, we send some
   * circuit-level sendme cells to indicate that we're willing to accept
   * more. */
  int deliver_window;

  /** Temporary field used during circuits_handle_oom. */
  uint32_t age_tmp;

  /** For storage while n_chan is pending (state CIRCUIT_STATE_CHAN_WAIT). */
  struct create_cell_t *n_chan_create_cell;

  /** When did circuit construction actually begin (ie send the
   * CREATE cell or begin cannibalization).
   *
   * Note: This timer will get reset if we decide to cannibalize
   * a circuit. It may also get reset during certain phases of hidden
   * service circuit use.
   *
   * We keep this timestamp with a higher resolution than most so that the
   * circuit-build-time tracking code can get millisecond resolution.
   */
  struct timeval timestamp_began;

  /** This timestamp marks when the init_circuit_base constructor ran. */
  struct timeval timestamp_created;

  /** When the circuit was first used, or 0 if the circuit is clean.
   *
   * XXXX023 Note that some code will artifically adjust this value backward
   * in time in order to indicate that a circuit shouldn't be used for new
   * streams, but that it can stay alive as long as it has streams on it.
   * That's a kludge we should fix.
   *
   * XXX023 The CBT code uses this field to record when HS-related
   * circuits entered certain states.  This usage probably won't
   * interfere with this field's primary purpose, but we should
   * document it more thoroughly to make sure of that.
   */
  time_t timestamp_dirty;

  uint16_t marked_for_close; /**< Should we close this circuit at the end of
                              * the main loop? (If true, holds the line number
                              * where this circuit was marked.) */
  const char *marked_for_close_file; /**< For debugging: in which file was this
                                      * circuit marked for close? */

  /** Unique ID for measuring tunneled network status requests. */
  uint64_t dirreq_id;

  /** Next circuit in linked list of all circuits (global_circuitlist). */
  TOR_LIST_ENTRY(circuit_t) head;

  /** Next circuit in the doubly-linked ring of circuits waiting to add
   * cells to n_conn.  NULL if we have no cells pending, or if we're not
   * linked to an OR connection. */
  struct circuit_t *next_active_on_n_chan;
  /** Previous circuit in the doubly-linked ring of circuits waiting to add
   * cells to n_conn.  NULL if we have no cells pending, or if we're not
   * linked to an OR connection. */
  struct circuit_t *prev_active_on_n_chan;

  /** Various statistics about cells being added to or removed from this
   * circuit's queues; used only if CELL_STATS events are enabled and
   * cleared after being sent to control port. */
  smartlist_t *testing_cell_stats;
} circuit_t;

/** Largest number of relay_early cells that we can send on a given
 * circuit. */
#define MAX_RELAY_EARLY_CELLS_PER_CIRCUIT 8

/**
 * Describes the circuit building process in simplified terms based
 * on the path bias accounting state for a circuit.
 *
 * NOTE: These state values are enumerated in the order for which we
 * expect circuits to transition through them. If you add states,
 * you need to preserve this overall ordering. The various pathbias
 * state transition and accounting functions (pathbias_mark_* and
 * pathbias_count_*) contain ordinal comparisons to enforce proper
 * state transitions for corrections.
 *
 * This state machine and the associated logic was created to prevent
 * miscounting due to unknown cases of circuit reuse. See also tickets
 * #6475 and #7802.
 */
typedef enum {
    /** This circuit is "new". It has not yet completed a first hop
     * or been counted by the path bias code. */
    PATH_STATE_NEW_CIRC = 0,
    /** This circuit has completed one/two hops, and has been counted by
     * the path bias logic. */
    PATH_STATE_BUILD_ATTEMPTED = 1,
    /** This circuit has been completely built */
    PATH_STATE_BUILD_SUCCEEDED = 2,
    /** Did we try to attach any SOCKS streams or hidserv introductions to
      * this circuit?
      *
      * Note: If we ever implement end-to-end stream timing through test
      * stream probes (#5707), we must *not* set this for those probes
      * (or any other automatic streams) because the adversary could
      * just tag at a later point.
      */
    PATH_STATE_USE_ATTEMPTED = 3,
    /** Did any SOCKS streams or hidserv introductions actually succeed on
      * this circuit?
      *
      * If any streams detatch/fail from this circuit, the code transitions
      * the circuit back to PATH_STATE_USE_ATTEMPTED to ensure we probe. See
      * pathbias_mark_use_rollback() for that.
      */
    PATH_STATE_USE_SUCCEEDED = 4,

    /**
     * This is a special state to indicate that we got a corrupted
     * relay cell on a circuit and we don't intend to probe it.
     */
    PATH_STATE_USE_FAILED = 5,

    /**
     * This is a special state to indicate that we already counted
     * the circuit. Used to guard against potential state machine
     * violations.
     */
    PATH_STATE_ALREADY_COUNTED = 6,
} path_state_t;
#define path_state_bitfield_t ENUM_BF(path_state_t)

/** An origin_circuit_t holds data necessary to build and use a circuit.
 */
typedef struct origin_circuit_t {
  circuit_t base_;

  /** Linked list of AP streams (or EXIT streams if hidden service)
   * associated with this circuit. */
  edge_connection_t *p_streams;

  /** Bytes read from any attached stream since last call to
   * control_event_circ_bandwidth_used().  Only used if we're configured
   * to emit CIRC_BW events. */
  uint32_t n_read_circ_bw;

  /** Bytes written to any attached stream since last call to
   * control_event_circ_bandwidth_used().  Only used if we're configured
   * to emit CIRC_BW events. */
  uint32_t n_written_circ_bw;

  /** Build state for this circuit. It includes the intended path
   * length, the chosen exit router, rendezvous information, etc.
   */
  cpath_build_state_t *build_state;
  /** The doubly-linked list of crypt_path_t entries, one per hop,
   * for this circuit. This includes ciphers for each hop,
   * integrity-checking digests for each hop, and package/delivery
   * windows for each hop.
   */
  crypt_path_t *cpath;

  /** Holds all rendezvous data on either client or service side. */
  rend_data_t *rend_data;

  /** How many more relay_early cells can we send on this circuit, according
   * to the specification? */
  unsigned int remaining_relay_early_cells : 4;

  /** Set if this circuit is insanely old and we already informed the user */
  unsigned int is_ancient : 1;

  /** Set if this circuit has already been opened. Used to detect
   * cannibalized circuits. */
  unsigned int has_opened : 1;

  /**
   * Path bias state machine. Used to ensure integrity of our
   * circuit building and usage accounting. See path_state_t
   * for more details.
   */
  path_state_bitfield_t path_state : 3;

  /* If this flag is set, we should not consider attaching any more
   * connections to this circuit. */
  unsigned int unusable_for_new_conns : 1;

  /**
   * Tristate variable to guard against pathbias miscounting
   * due to circuit purpose transitions changing the decision
   * of pathbias_should_count(). This variable is informational
   * only. The current results of pathbias_should_count() are
   * the official decision for pathbias accounting.
   */
  uint8_t pathbias_shouldcount;
#define PATHBIAS_SHOULDCOUNT_UNDECIDED 0
#define PATHBIAS_SHOULDCOUNT_IGNORED   1
#define PATHBIAS_SHOULDCOUNT_COUNTED   2

  /** For path probing. Store the temporary probe stream ID
   * for response comparison */
  streamid_t pathbias_probe_id;

  /** For path probing. Store the temporary probe address nonce
   * (in host byte order) for response comparison. */
  uint32_t pathbias_probe_nonce;

  /** Set iff this is a hidden-service circuit which has timed out
   * according to our current circuit-build timeout, but which has
   * been kept around because it might still succeed in connecting to
   * its destination, and which is not a fully-connected rendezvous
   * circuit.
   *
   * (We clear this flag for client-side rendezvous circuits when they
   * are 'joined' to the other side's rendezvous circuit, so that
   * connection_ap_handshake_attach_circuit can put client streams on
   * the circuit.  We also clear this flag for service-side rendezvous
   * circuits when they are 'joined' to a client's rend circ, but only
   * for symmetry with the client case.  Client-side introduction
   * circuits are closed when we get a joined rend circ, and
   * service-side introduction circuits never have this flag set.) */
  unsigned int hs_circ_has_timed_out : 1;

  /** Set iff this circuit has been given a relaxed timeout because
   * no circuits have opened. Used to prevent spamming logs. */
  unsigned int relaxed_timeout : 1;

  /** Set iff this is a service-side rendezvous circuit for which a
   * new connection attempt has been launched.  We consider launching
   * a new service-side rend circ to a client when the previous one
   * fails; now that we don't necessarily close a service-side rend
   * circ when we launch a new one to the same client, this flag keeps
   * us from launching two retries for the same failed rend circ. */
  unsigned int hs_service_side_rend_circ_has_been_relaunched : 1;

  /** What commands were sent over this circuit that decremented the
   * RELAY_EARLY counter? This is for debugging task 878. */
  uint8_t relay_early_commands[MAX_RELAY_EARLY_CELLS_PER_CIRCUIT];

  /** How many RELAY_EARLY cells have been sent over this circuit? This is
   * for debugging task 878, too. */
  int relay_early_cells_sent;

  /** The next stream_id that will be tried when we're attempting to
   * construct a new AP stream originating at this circuit. */
  streamid_t next_stream_id;

  /* The intro key replaces the hidden service's public key if purpose is
   * S_ESTABLISH_INTRO or S_INTRO, provided that no unversioned rendezvous
   * descriptor is used. */
  crypto_pk_t *intro_key;

  /** Quasi-global identifier for this circuit; used for control.c */
  /* XXXX NM This can get re-used after 2**32 circuits. */
  uint32_t global_identifier;

  /** True if we have associated one stream to this circuit, thereby setting
   * the isolation paramaters for this circuit.  Note that this doesn't
   * necessarily mean that we've <em>attached</em> any streams to the circuit:
   * we may only have marked up this circuit during the launch process.
   */
  unsigned int isolation_values_set : 1;
  /** True iff any stream has <em>ever</em> been attached to this circuit.
   *
   * In a better world we could use timestamp_dirty for this, but
   * timestamp_dirty is far too overloaded at the moment.
   */
  unsigned int isolation_any_streams_attached : 1;

  /** A bitfield of ISO_* flags for every isolation field such that this
   * circuit has had streams with more than one value for that field
   * attached to it. */
  uint8_t isolation_flags_mixed;

  /** @name Isolation parameters
   *
   * If any streams have been associated with this circ (isolation_values_set
   * == 1), and all streams associated with the circuit have had the same
   * value for some field ((isolation_flags_mixed & ISO_FOO) == 0), then these
   * elements hold the value for that field.
   *
   * Note again that "associated" is not the same as "attached": we
   * preliminarily associate streams with a circuit while the circuit is being
   * launched, so that we can tell whether we need to launch more circuits.
   *
   * @{
   */
  uint8_t client_proto_type;
  uint8_t client_proto_socksver;
  uint16_t dest_port;
  tor_addr_t client_addr;
  char *dest_address;
  int session_group;
  unsigned nym_epoch;
  size_t socks_username_len;
  uint8_t socks_password_len;
  /* Note that the next two values are NOT NUL-terminated; see
     socks_username_len and socks_password_len for their lengths. */
  char *socks_username;
  char *socks_password;
  /** Global identifier for the first stream attached here; used by
   * ISO_STREAM. */
  uint64_t associated_isolated_stream_global_id;
  /**@}*/
  /** A list of addr_policy_t for this circuit in particular. Used by
   * adjust_exit_policy_from_exitpolicy_failure.
   */
  smartlist_t *prepend_policy;
} origin_circuit_t;

struct onion_queue_t;

/** An or_circuit_t holds information needed to implement a circuit at an
 * OR. */
typedef struct or_circuit_t {
  circuit_t base_;

  /** Next circuit in the doubly-linked ring of circuits waiting to add
   * cells to p_chan.  NULL if we have no cells pending, or if we're not
   * linked to an OR connection. */
  struct circuit_t *next_active_on_p_chan;
  /** Previous circuit in the doubly-linked ring of circuits waiting to add
   * cells to p_chan.  NULL if we have no cells pending, or if we're not
   * linked to an OR connection. */
  struct circuit_t *prev_active_on_p_chan;
  /** Pointer to an entry on the onion queue, if this circuit is waiting for a
   * chance to give an onionskin to a cpuworker. Used only in onion.c */
  struct onion_queue_t *onionqueue_entry;

  /** The circuit_id used in the previous (backward) hop of this circuit. */
  circid_t p_circ_id;
  /** Queue of cells waiting to be transmitted on p_conn. */
  cell_queue_t p_chan_cells;
  /** The channel that is previous in this circuit. */
  channel_t *p_chan;
  /**
   * Circuit mux associated with p_chan to which this circuit is attached;
   * NULL if we have no p_chan.
   */
  circuitmux_t *p_mux;
  /** Linked list of Exit streams associated with this circuit. */
  edge_connection_t *n_streams;
  /** Linked list of Exit streams associated with this circuit that are
   * still being resolved. */
  edge_connection_t *resolving_streams;
  /** The cipher used by intermediate hops for cells heading toward the
   * OP. */
  crypto_cipher_t *p_crypto;
  /** The cipher used by intermediate hops for cells heading away from
   * the OP. */
  crypto_cipher_t *n_crypto;

  /** The integrity-checking digest used by intermediate hops, for
   * cells packaged here and heading towards the OP.
   */
  crypto_digest_t *p_digest;
  /** The integrity-checking digest used by intermediate hops, for
   * cells packaged at the OP and arriving here.
   */
  crypto_digest_t *n_digest;

  /** Points to spliced circuit if purpose is REND_ESTABLISHED, and circuit
   * is not marked for close. */
  struct or_circuit_t *rend_splice;

  struct or_circuit_rendinfo_s *rendinfo;

  /** Stores KH for the handshake. */
  char rend_circ_nonce[DIGEST_LEN];/* KH in tor-spec.txt */

  /** How many more relay_early cells can we send on this circuit, according
   * to the specification? */
  unsigned int remaining_relay_early_cells : 4;

  /** True iff this circuit was made with a CREATE_FAST cell. */
  unsigned int is_first_hop : 1;

  /** Number of cells that were removed from circuit queue; reset every
   * time when writing buffer stats to disk. */
  uint32_t processed_cells;

  /** Total time in milliseconds that cells spent in both app-ward and
   * exit-ward queues of this circuit; reset every time when writing
   * buffer stats to disk. */
  uint64_t total_cell_waiting_time;

  /** Maximum cell queue size for a middle relay; this is stored per circuit
   * so append_cell_to_circuit_queue() can adjust it if it changes.  If set
   * to zero, it is initialized to the default value.
   */
  uint32_t max_middle_cells;
} or_circuit_t;

typedef struct or_circuit_rendinfo_s {

#if REND_COOKIE_LEN != DIGEST_LEN
#error "The REND_TOKEN_LEN macro assumes REND_COOKIE_LEN == DIGEST_LEN"
#endif
#define REND_TOKEN_LEN DIGEST_LEN

  /** A hash of location-hidden service's PK if purpose is INTRO_POINT, or a
   * rendezvous cookie if purpose is REND_POINT_WAITING. Filled with zeroes
   * otherwise.
   */
  char rend_token[REND_TOKEN_LEN];

  /** True if this is a rendezvous point circuit; false if this is an
   * introduction point. */
  unsigned is_rend_circ;

} or_circuit_rendinfo_t;

/** Convert a circuit subtype to a circuit_t. */
#define TO_CIRCUIT(x)  (&((x)->base_))

/** Convert a circuit_t* to a pointer to the enclosing or_circuit_t.  Assert
 * if the cast is impossible. */
static or_circuit_t *TO_OR_CIRCUIT(circuit_t *);
static const or_circuit_t *CONST_TO_OR_CIRCUIT(const circuit_t *);
/** Convert a circuit_t* to a pointer to the enclosing origin_circuit_t.
 * Assert if the cast is impossible. */
static origin_circuit_t *TO_ORIGIN_CIRCUIT(circuit_t *);
static const origin_circuit_t *CONST_TO_ORIGIN_CIRCUIT(const circuit_t *);

static INLINE or_circuit_t *TO_OR_CIRCUIT(circuit_t *x)
{
  tor_assert(x->magic == OR_CIRCUIT_MAGIC);
  return DOWNCAST(or_circuit_t, x);
}
static INLINE const or_circuit_t *CONST_TO_OR_CIRCUIT(const circuit_t *x)
{
  tor_assert(x->magic == OR_CIRCUIT_MAGIC);
  return DOWNCAST(or_circuit_t, x);
}
static INLINE origin_circuit_t *TO_ORIGIN_CIRCUIT(circuit_t *x)
{
  tor_assert(x->magic == ORIGIN_CIRCUIT_MAGIC);
  return DOWNCAST(origin_circuit_t, x);
}
static INLINE const origin_circuit_t *CONST_TO_ORIGIN_CIRCUIT(
    const circuit_t *x)
{
  tor_assert(x->magic == ORIGIN_CIRCUIT_MAGIC);
  return DOWNCAST(origin_circuit_t, x);
}

/** Bitfield type: things that we're willing to use invalid routers for. */
typedef enum invalid_router_usage_t {
  ALLOW_INVALID_ENTRY       =1,
  ALLOW_INVALID_EXIT        =2,
  ALLOW_INVALID_MIDDLE      =4,
  ALLOW_INVALID_RENDEZVOUS  =8,
  ALLOW_INVALID_INTRODUCTION=16,
} invalid_router_usage_t;

/* limits for TCP send and recv buffer size used for constrained sockets */
#define MIN_CONSTRAINED_TCP_BUFFER 2048
#define MAX_CONSTRAINED_TCP_BUFFER 262144  /* 256k */

/** @name Isolation flags

    Ways to isolate client streams

    @{
*/
/** Isolate based on destination port */
#define ISO_DESTPORT    (1u<<0)
/** Isolate based on destination address */
#define ISO_DESTADDR    (1u<<1)
/** Isolate based on SOCKS authentication */
#define ISO_SOCKSAUTH   (1u<<2)
/** Isolate based on client protocol choice */
#define ISO_CLIENTPROTO (1u<<3)
/** Isolate based on client address */
#define ISO_CLIENTADDR  (1u<<4)
/** Isolate based on session group (always on). */
#define ISO_SESSIONGRP  (1u<<5)
/** Isolate based on newnym epoch (always on). */
#define ISO_NYM_EPOCH   (1u<<6)
/** Isolate all streams (Internal only). */
#define ISO_STREAM      (1u<<7)
/**@}*/

/** Default isolation level for ports. */
#define ISO_DEFAULT (ISO_CLIENTADDR|ISO_SOCKSAUTH|ISO_SESSIONGRP|ISO_NYM_EPOCH)

/** Indicates that we haven't yet set a session group on a port_cfg_t. */
#define SESSION_GROUP_UNSET -1
/** Session group reserved for directory connections */
#define SESSION_GROUP_DIRCONN -2
/** Session group reserved for resolve requests launched by a controller */
#define SESSION_GROUP_CONTROL_RESOLVE -3
/** First automatically allocated session group number */
#define SESSION_GROUP_FIRST_AUTO -4

/** Configuration for a single port that we're listening on. */
typedef struct port_cfg_t {
  tor_addr_t addr; /**< The actual IP to listen on, if !is_unix_addr. */
  int port; /**< The configured port, or CFG_AUTO_PORT to tell Tor to pick its
             * own port. */
  uint8_t type; /**< One of CONN_TYPE_*_LISTENER */
  unsigned is_unix_addr : 1; /**< True iff this is an AF_UNIX address. */

  /* Client port types (socks, dns, trans, natd) only: */
  uint8_t isolation_flags; /**< Zero or more isolation flags */
  int session_group; /**< A session group, or -1 if this port is not in a
                      * session group. */
  /* Socks only: */
  /** When both no-auth and user/pass are advertised by a SOCKS client, select
   * no-auth. */
  unsigned int socks_prefer_no_auth : 1;

  /* Server port types (or, dir) only: */
  unsigned int no_advertise : 1;
  unsigned int no_listen : 1;
  unsigned int all_addrs : 1;
  unsigned int bind_ipv4_only : 1;
  unsigned int bind_ipv6_only : 1;

  /* Client port types only: */
  unsigned int ipv4_traffic : 1;
  unsigned int ipv6_traffic : 1;
  unsigned int prefer_ipv6 : 1;

  /** For a socks listener: should we cache IPv4/IPv6 DNS information that
   * exit nodes tell us?
   *
   * @{ */
  unsigned int cache_ipv4_answers : 1;
  unsigned int cache_ipv6_answers : 1;
  /** @} */
  /** For a socks listeners: if we find an answer in our client-side DNS cache,
   * should we use it?
   *
   * @{ */
  unsigned int use_cached_ipv4_answers : 1;
  unsigned int use_cached_ipv6_answers : 1;
  /** @} */
  /** For socks listeners: When we can automap an address to IPv4 or IPv6,
   * do we prefer IPv6? */
  unsigned int prefer_ipv6_virtaddr : 1;

  /* Unix sockets only: */
  /** Path for an AF_UNIX address */
  char unix_addr[FLEXIBLE_ARRAY_MEMBER];
} port_cfg_t;

/** Ordinary configuration line. */
#define CONFIG_LINE_NORMAL 0
/** Appends to previous configuration for the same option, even if we
 * would ordinary replace it. */
#define CONFIG_LINE_APPEND 1
/* Removes all previous configuration for an option. */
#define CONFIG_LINE_CLEAR 2

/** A linked list of lines in a config file. */
typedef struct config_line_t {
  char *key;
  char *value;
  struct config_line_t *next;
  /** What special treatment (if any) does this line require? */
  unsigned int command:2;
  /** If true, subsequent assignments to this linelist should replace
   * it, not extend it.  Set only on the first item in a linelist in an
   * or_options_t. */
  unsigned int fragile:1;
} config_line_t;

typedef struct routerset_t routerset_t;

/** A magic value for the (Socks|OR|...)Port options below, telling Tor
 * to pick its own port. */
#define CFG_AUTO_PORT 0xc4005e

/** Configuration options for a Tor process. */
typedef struct {
  uint32_t magic_;

  /** What should the tor process actually do? */
  enum {
    CMD_RUN_TOR=0, CMD_LIST_FINGERPRINT, CMD_HASH_PASSWORD,
    CMD_VERIFY_CONFIG, CMD_RUN_UNITTESTS, CMD_DUMP_CONFIG
  } command;
  char *command_arg; /**< Argument for command-line option. */

  config_line_t *Logs; /**< New-style list of configuration lines
                        * for logs */
  int LogTimeGranularity; /**< Log resolution in milliseconds. */

  int LogMessageDomains; /**< Boolean: Should we log the domain(s) in which
                          * each log message occurs? */

  char *DebugLogFile; /**< Where to send verbose log messages. */
  char *DataDirectory; /**< OR only: where to store long-term data. */
  char *Nickname; /**< OR only: nickname of this onion router. */
  char *Address; /**< OR only: configured address for this onion router. */
  char *PidFile; /**< Where to store PID of Tor process. */

  int DynamicDHGroups; /**< Dynamic generation of prime moduli for use in DH.*/

  routerset_t *ExitNodes; /**< Structure containing nicknames, digests,
                           * country codes and IP address patterns of ORs to
                           * consider as exits. */
  routerset_t *EntryNodes;/**< Structure containing nicknames, digests,
                           * country codes and IP address patterns of ORs to
                           * consider as entry points. */
  int StrictNodes; /**< Boolean: When none of our EntryNodes or ExitNodes
                    * are up, or we need to access a node in ExcludeNodes,
                    * do we just fail instead? */
  routerset_t *ExcludeNodes;/**< Structure containing nicknames, digests,
                             * country codes and IP address patterns of ORs
                             * not to use in circuits. But see StrictNodes
                             * above. */
  routerset_t *ExcludeExitNodes;/**< Structure containing nicknames, digests,
                                 * country codes and IP address patterns of
                                 * ORs not to consider as exits. */

  /** Union of ExcludeNodes and ExcludeExitNodes */
  routerset_t *ExcludeExitNodesUnion_;

  int DisableAllSwap; /**< Boolean: Attempt to call mlockall() on our
                       * process for all current and future memory. */

  /** List of "entry", "middle", "exit", "introduction", "rendezvous". */
  smartlist_t *AllowInvalidNodes;
  /** Bitmask; derived from AllowInvalidNodes. */
  invalid_router_usage_t AllowInvalid_;
  config_line_t *ExitPolicy; /**< Lists of exit policy components. */
  int ExitPolicyRejectPrivate; /**< Should we not exit to local addresses? */
  config_line_t *SocksPolicy; /**< Lists of socks policy components */
  config_line_t *DirPolicy; /**< Lists of dir policy components */
  /** Addresses to bind for listening for SOCKS connections. */
  config_line_t *SocksListenAddress;
  /** Addresses to bind for listening for transparent pf/netfilter
   * connections. */
  config_line_t *TransListenAddress;
  /** Addresses to bind for listening for transparent natd connections */
  config_line_t *NATDListenAddress;
  /** Addresses to bind for listening for SOCKS connections. */
  config_line_t *DNSListenAddress;
  /** Addresses to bind for listening for OR connections. */
  config_line_t *ORListenAddress;
  /** Addresses to bind for listening for directory connections. */
  config_line_t *DirListenAddress;
  /** Addresses to bind for listening for control connections. */
  config_line_t *ControlListenAddress;
  /** Local address to bind outbound sockets */
  config_line_t *OutboundBindAddress;
  /** IPv4 address derived from OutboundBindAddress. */
  tor_addr_t OutboundBindAddressIPv4_;
  /** IPv6 address derived from OutboundBindAddress. */
  tor_addr_t OutboundBindAddressIPv6_;
  /** Directory server only: which versions of
   * Tor should we tell users to run? */
  config_line_t *RecommendedVersions;
  config_line_t *RecommendedClientVersions;
  config_line_t *RecommendedServerVersions;
  /** Whether dirservers allow router descriptors with private IPs. */
  int DirAllowPrivateAddresses;
  /** Whether routers accept EXTEND cells to routers with private IPs. */
  int ExtendAllowPrivateAddresses;
  char *User; /**< Name of user to run Tor as. */
  char *Group; /**< Name of group to run Tor as. */
  config_line_t *ORPort_lines; /**< Ports to listen on for OR connections. */
  /** Ports to listen on for extended OR connections. */
  config_line_t *ExtORPort_lines;
  /** Ports to listen on for SOCKS connections. */
  config_line_t *SocksPort_lines;
  /** Ports to listen on for transparent pf/netfilter connections. */
  config_line_t *TransPort_lines;
  const char *TransProxyType; /**< What kind of transparent proxy
                               * implementation are we using? */
  /** Parsed value of TransProxyType. */
  enum {
    TPT_DEFAULT,
    TPT_PF_DIVERT,
    TPT_IPFW,
    TPT_TPROXY,
  } TransProxyType_parsed;
  config_line_t *NATDPort_lines; /**< Ports to listen on for transparent natd
                            * connections. */
  config_line_t *ControlPort_lines; /**< Ports to listen on for control
                               * connections. */
  config_line_t *ControlSocket; /**< List of Unix Domain Sockets to listen on
                                 * for control connections. */

  int ControlSocketsGroupWritable; /**< Boolean: Are control sockets g+rw? */
  /** Ports to listen on for directory connections. */
  config_line_t *DirPort_lines;
  config_line_t *DNSPort_lines; /**< Ports to listen on for DNS requests. */

  /* MaxMemInQueues value as input by the user. We clean this up to be
   * MaxMemInQueues. */
  uint64_t MaxMemInQueues_raw;
  uint64_t MaxMemInQueues;/**< If we have more memory than this allocated
                            * for queues and buffers, run the OOM handler */

  /** @name port booleans
   *
   * Derived booleans: True iff there is a non-listener port on an AF_INET or
   * AF_INET6 address of the given type configured in one of the _lines
   * options above.
   *
   * @{
   */
  unsigned int ORPort_set : 1;
  unsigned int SocksPort_set : 1;
  unsigned int TransPort_set : 1;
  unsigned int NATDPort_set : 1;
  unsigned int ControlPort_set : 1;
  unsigned int DirPort_set : 1;
  unsigned int DNSPort_set : 1;
  unsigned int ExtORPort_set : 1;
  /**@}*/

  int AssumeReachable; /**< Whether to publish our descriptor regardless. */
  int AuthoritativeDir; /**< Boolean: is this an authoritative directory? */
  int V3AuthoritativeDir; /**< Boolean: is this an authoritative directory
                           * for version 3 directories? */
  int NamingAuthoritativeDir; /**< Boolean: is this an authoritative directory
                               * that's willing to bind names? */
  int VersioningAuthoritativeDir; /**< Boolean: is this an authoritative
                                   * directory that's willing to recommend
                                   * versions? */
  int BridgeAuthoritativeDir; /**< Boolean: is this an authoritative directory
                               * that aggregates bridge descriptors? */

  /** If set on a bridge authority, it will answer requests on its dirport
   * for bridge statuses -- but only if the requests use this password. */
  char *BridgePassword;
  /** If BridgePassword is set, this is a SHA256 digest of the basic http
   * authenticator for it. Used so we can do a time-independent comparison. */
  char *BridgePassword_AuthDigest_;

  int UseBridges; /**< Boolean: should we start all circuits with a bridge? */
  config_line_t *Bridges; /**< List of bootstrap bridge addresses. */

  config_line_t *ClientTransportPlugin; /**< List of client
                                           transport plugins. */

  config_line_t *ServerTransportPlugin; /**< List of client
                                           transport plugins. */

  /** List of TCP/IP addresses that transports should listen at. */
  config_line_t *ServerTransportListenAddr;

  /** List of options that must be passed to pluggable transports. */
  config_line_t *ServerTransportOptions;

  int BridgeRelay; /**< Boolean: are we acting as a bridge relay? We make
                    * this explicit so we can change how we behave in the
                    * future. */

  /** Boolean: if we know the bridge's digest, should we get new
   * descriptors from the bridge authorities or from the bridge itself? */
  int UpdateBridgesFromAuthority;

  int AvoidDiskWrites; /**< Boolean: should we never cache things to disk?
                        * Not used yet. */
  int ClientOnly; /**< Boolean: should we never evolve into a server role? */
  /** To what authority types do we publish our descriptor? Choices are
   * "v1", "v2", "v3", "bridge", or "". */
  smartlist_t *PublishServerDescriptor;
  /** A bitfield of authority types, derived from PublishServerDescriptor. */
  dirinfo_type_t PublishServerDescriptor_;
  /** Boolean: do we publish hidden service descriptors to the HS auths? */
  int PublishHidServDescriptors;
  int FetchServerDescriptors; /**< Do we fetch server descriptors as normal? */
  int FetchHidServDescriptors; /**< and hidden service descriptors? */
  int HidServDirectoryV2; /**< Do we participate in the HS DHT? */

  int VoteOnHidServDirectoriesV2; /**< As a directory authority, vote on
                                   * assignment of the HSDir flag? */
  int MinUptimeHidServDirectoryV2; /**< As directory authority, accept hidden
                                    * service directories after what time? */

  int FetchUselessDescriptors; /**< Do we fetch non-running descriptors too? */
  int AllDirActionsPrivate; /**< Should every directory action be sent
                             * through a Tor circuit? */

  /** Run in 'tor2web mode'? (I.e. only make client connections to hidden
   * services, and use a single hop for all hidden-service-related
   * circuits.) */
  int Tor2webMode;

  /** Close hidden service client circuits immediately when they reach
   * the normal circuit-build timeout, even if they have already sent
   * an INTRODUCE1 cell on its way to the service. */
  int CloseHSClientCircuitsImmediatelyOnTimeout;

  /** Close hidden-service-side rendezvous circuits immediately when
   * they reach the normal circuit-build timeout. */
  int CloseHSServiceRendCircuitsImmediatelyOnTimeout;

  int ConnLimit; /**< Demanded minimum number of simultaneous connections. */
  int ConnLimit_; /**< Maximum allowed number of simultaneous connections. */
  int RunAsDaemon; /**< If true, run in the background. (Unix only) */
  int FascistFirewall; /**< Whether to prefer ORs reachable on open ports. */
  smartlist_t *FirewallPorts; /**< Which ports our firewall allows
                               * (strings). */
  config_line_t *ReachableAddresses; /**< IP:ports our firewall allows. */
  config_line_t *ReachableORAddresses; /**< IP:ports for OR conns. */
  config_line_t *ReachableDirAddresses; /**< IP:ports for Dir conns. */

  int ConstrainedSockets; /**< Shrink xmit and recv socket buffers. */
  uint64_t ConstrainedSockSize; /**< Size of constrained buffers. */

  /** Whether we should drop exit streams from Tors that we don't know are
   * relays.  One of "0" (never refuse), "1" (always refuse), or "-1" (do
   * what the consensus says, defaulting to 'refuse' if the consensus says
   * nothing). */
  int RefuseUnknownExits;

  /** Application ports that require all nodes in circ to have sufficient
   * uptime. */
  smartlist_t *LongLivedPorts;
  /** Application ports that are likely to be unencrypted and
   * unauthenticated; we reject requests for them to prevent the
   * user from screwing up and leaking plaintext secrets to an
   * observer somewhere on the Internet. */
  smartlist_t *RejectPlaintextPorts;
  /** Related to RejectPlaintextPorts above, except this config option
   * controls whether we warn (in the log and via a controller status
   * event) every time a risky connection is attempted. */
  smartlist_t *WarnPlaintextPorts;
  /** Should we try to reuse the same exit node for a given host */
  smartlist_t *TrackHostExits;
  int TrackHostExitsExpire; /**< Number of seconds until we expire an
                             * addressmap */
  config_line_t *AddressMap; /**< List of address map directives. */
  int AutomapHostsOnResolve; /**< If true, when we get a resolve request for a
                              * hostname ending with one of the suffixes in
                              * <b>AutomapHostsSuffixes</b>, map it to a
                              * virtual address. */
  smartlist_t *AutomapHostsSuffixes; /**< List of suffixes for
                                      * <b>AutomapHostsOnResolve</b>. */
  int RendPostPeriod; /**< How often do we post each rendezvous service
                       * descriptor? Remember to publish them independently. */
  int KeepalivePeriod; /**< How often do we send padding cells to keep
                        * connections alive? */
  int SocksTimeout; /**< How long do we let a socks connection wait
                     * unattached before we fail it? */
  int LearnCircuitBuildTimeout; /**< If non-zero, we attempt to learn a value
                                 * for CircuitBuildTimeout based on timeout
                                 * history */
  int CircuitBuildTimeout; /**< Cull non-open circuits that were born at
                            * least this many seconds ago. Used until
                            * adaptive algorithm learns a new value. */
  int CircuitIdleTimeout; /**< Cull open clean circuits that were born
                           * at least this many seconds ago. */
  int CircuitStreamTimeout; /**< If non-zero, detach streams from circuits
                             * and try a new circuit if the stream has been
                             * waiting for this many seconds. If zero, use
                             * our default internal timeout schedule. */
  int MaxOnionQueueDelay; /**<DOCDOC*/
  int NewCircuitPeriod; /**< How long do we use a circuit before building
                         * a new one? */
  int MaxCircuitDirtiness; /**< Never use circs that were first used more than
                                this interval ago. */
  int PredictedPortsRelevanceTime; /** How long after we've requested a
                                    * connection for a given port, do we want
                                    * to continue to pick exits that support
                                    * that port?  */
  uint64_t BandwidthRate; /**< How much bandwidth, on average, are we willing
                           * to use in a second? */
  uint64_t BandwidthBurst; /**< How much bandwidth, at maximum, are we willing
                            * to use in a second? */
  uint64_t MaxAdvertisedBandwidth; /**< How much bandwidth are we willing to
                                    * tell people we have? */
  uint64_t RelayBandwidthRate; /**< How much bandwidth, on average, are we
                                 * willing to use for all relayed conns? */
  uint64_t RelayBandwidthBurst; /**< How much bandwidth, at maximum, will we
                                 * use in a second for all relayed conns? */
  uint64_t PerConnBWRate; /**< Long-term bw on a single TLS conn, if set. */
  uint64_t PerConnBWBurst; /**< Allowed burst on a single TLS conn, if set. */
  int NumCPUs; /**< How many CPUs should we try to use? */
//int RunTesting; /**< If true, create testing circuits to measure how well the
//                 * other ORs are running. */
  config_line_t *RendConfigLines; /**< List of configuration lines
                                          * for rendezvous services. */
  config_line_t *HidServAuth; /**< List of configuration lines for client-side
                               * authorizations for hidden services */
  char *ContactInfo; /**< Contact info to be published in the directory. */

  int HeartbeatPeriod; /**< Log heartbeat messages after this many seconds
                        * have passed. */

  char *HTTPProxy; /**< hostname[:port] to use as http proxy, if any. */
  tor_addr_t HTTPProxyAddr; /**< Parsed IPv4 addr for http proxy, if any. */
  uint16_t HTTPProxyPort; /**< Parsed port for http proxy, if any. */
  char *HTTPProxyAuthenticator; /**< username:password string, if any. */

  char *HTTPSProxy; /**< hostname[:port] to use as https proxy, if any. */
  tor_addr_t HTTPSProxyAddr; /**< Parsed addr for https proxy, if any. */
  uint16_t HTTPSProxyPort; /**< Parsed port for https proxy, if any. */
  char *HTTPSProxyAuthenticator; /**< username:password string, if any. */

  char *Socks4Proxy; /**< hostname:port to use as a SOCKS4 proxy, if any. */
  tor_addr_t Socks4ProxyAddr; /**< Derived from Socks4Proxy. */
  uint16_t Socks4ProxyPort; /**< Derived from Socks4Proxy. */

  char *Socks5Proxy; /**< hostname:port to use as a SOCKS5 proxy, if any. */
  tor_addr_t Socks5ProxyAddr; /**< Derived from Sock5Proxy. */
  uint16_t Socks5ProxyPort; /**< Derived from Socks5Proxy. */
  char *Socks5ProxyUsername; /**< Username for SOCKS5 authentication, if any */
  char *Socks5ProxyPassword; /**< Password for SOCKS5 authentication, if any */

  /** List of configuration lines for replacement directory authorities.
   * If you just want to replace one class of authority at a time,
   * use the "Alternate*Authority" options below instead. */
  config_line_t *DirAuthorities;

  /** List of fallback directory servers */
  config_line_t *FallbackDir;

  /** Weight to apply to all directory authority rates if considering them
   * along with fallbackdirs */
  double DirAuthorityFallbackRate;

  /** If set, use these main (currently v3) directory authorities and
   * not the default ones. */
  config_line_t *AlternateDirAuthority;

  /** If set, use these bridge authorities and not the default one. */
  config_line_t *AlternateBridgeAuthority;

  char *MyFamily; /**< Declared family for this OR. */
  config_line_t *NodeFamilies; /**< List of config lines for
                                * node families */
  smartlist_t *NodeFamilySets; /**< List of parsed NodeFamilies values. */
  config_line_t *AuthDirBadDir; /**< Address policy for descriptors to
                                 * mark as bad dir mirrors. */
  config_line_t *AuthDirBadExit; /**< Address policy for descriptors to
                                  * mark as bad exits. */
  config_line_t *AuthDirReject; /**< Address policy for descriptors to
                                 * reject. */
  config_line_t *AuthDirInvalid; /**< Address policy for descriptors to
                                  * never mark as valid. */
  /** @name AuthDir...CC
   *
   * Lists of country codes to mark as BadDir, BadExit, or Invalid, or to
   * reject entirely.
   *
   * @{
   */
  smartlist_t *AuthDirBadDirCCs;
  smartlist_t *AuthDirBadExitCCs;
  smartlist_t *AuthDirInvalidCCs;
  smartlist_t *AuthDirRejectCCs;
  /**@}*/

  int AuthDirListBadDirs; /**< True iff we should list bad dirs,
                           * and vote for all other dir mirrors as good. */
  int AuthDirListBadExits; /**< True iff we should list bad exits,
                            * and vote for all other exits as good. */
  int AuthDirRejectUnlisted; /**< Boolean: do we reject all routers that
                              * aren't named in our fingerprint file? */
  int AuthDirMaxServersPerAddr; /**< Do not permit more than this
                                 * number of servers per IP address. */
  int AuthDirMaxServersPerAuthAddr; /**< Do not permit more than this
                                     * number of servers per IP address shared
                                     * with an authority. */
  int AuthDirHasIPv6Connectivity; /**< Boolean: are we on IPv6?  */

  /** If non-zero, always vote the Fast flag for any relay advertising
   * this amount of capacity or more. */
  uint64_t AuthDirFastGuarantee;

  /** If non-zero, this advertised capacity or more is always sufficient
   * to satisfy the bandwidth requirement for the Guard flag. */
  uint64_t AuthDirGuardBWGuarantee;

  char *AccountingStart; /**< How long is the accounting interval, and when
                          * does it start? */
  uint64_t AccountingMax; /**< How many bytes do we allow per accounting
                           * interval before hibernation?  0 for "never
                           * hibernate." */

  /** Base64-encoded hash of accepted passwords for the control system. */
  config_line_t *HashedControlPassword;
  /** As HashedControlPassword, but not saved. */
  config_line_t *HashedControlSessionPassword;

  int CookieAuthentication; /**< Boolean: do we enable cookie-based auth for
                             * the control system? */
  char *CookieAuthFile; /**< Filesystem location of a ControlPort
                         *   authentication cookie. */
  char *ExtORPortCookieAuthFile; /**< Filesystem location of Extended
                                 *   ORPort authentication cookie. */
  int CookieAuthFileGroupReadable; /**< Boolean: Is the CookieAuthFile g+r? */
  int ExtORPortCookieAuthFileGroupReadable; /**< Boolean: Is the
                                             * ExtORPortCookieAuthFile g+r? */
  int LeaveStreamsUnattached; /**< Boolean: Does Tor attach new streams to
                          * circuits itself (0), or does it expect a controller
                          * to cope? (1) */
  int DisablePredictedCircuits; /**< Boolean: does Tor preemptively
                                 * make circuits in the background (0),
                                 * or not (1)? */

  /** Process specifier for a controller that ‘owns’ this Tor
   * instance.  Tor will terminate if its owning controller does. */
  char *OwningControllerProcess;

  int ShutdownWaitLength; /**< When we get a SIGINT and we're a server, how
                           * long do we wait before exiting? */
  char *SafeLogging; /**< Contains "relay", "1", "0" (meaning no scrubbing). */

  /* Derived from SafeLogging */
  enum {
    SAFELOG_SCRUB_ALL, SAFELOG_SCRUB_RELAY, SAFELOG_SCRUB_NONE
  } SafeLogging_;

  int Sandbox; /**< Boolean: should sandboxing be enabled? */
  int SafeSocks; /**< Boolean: should we outright refuse application
                  * connections that use socks4 or socks5-with-local-dns? */
#define LOG_PROTOCOL_WARN (get_options()->ProtocolWarnings ? \
                           LOG_WARN : LOG_INFO)
  int ProtocolWarnings; /**< Boolean: when other parties screw up the Tor
                         * protocol, is it a warn or an info in our logs? */
  int TestSocks; /**< Boolean: when we get a socks connection, do we loudly
                  * log whether it was DNS-leaking or not? */
  int HardwareAccel; /**< Boolean: Should we enable OpenSSL hardware
                      * acceleration where available? */
  /** Token Bucket Refill resolution in milliseconds. */
  int TokenBucketRefillInterval;
  char *AccelName; /**< Optional hardware acceleration engine name. */
  char *AccelDir; /**< Optional hardware acceleration engine search dir. */
  int UseEntryGuards; /**< Boolean: Do we try to enter from a smallish number
                       * of fixed nodes? */
  int NumEntryGuards; /**< How many entry guards do we try to establish? */
  int UseEntryGuardsAsDirGuards; /** Boolean: Do we try to get directory info
                                  * from a smallish number of fixed nodes? */
  int NumDirectoryGuards; /**< How many dir guards do we try to establish?
                           * If 0, use value from NumEntryGuards. */
  int RephistTrackTime; /**< How many seconds do we keep rephist info? */
  int FastFirstHopPK; /**< If Tor believes it is safe, should we save a third
                       * of our PK time by sending CREATE_FAST cells? */
  /** Should we always fetch our dir info on the mirror schedule (which
   * means directly from the authorities) no matter our other config? */
  int FetchDirInfoEarly;

  /** Should we fetch our dir info at the start of the consensus period? */
  int FetchDirInfoExtraEarly;

  char *VirtualAddrNetworkIPv4; /**< Address and mask to hand out for virtual
                                 * MAPADDRESS requests for IPv4 addresses */
  char *VirtualAddrNetworkIPv6; /**< Address and mask to hand out for virtual
                                 * MAPADDRESS requests for IPv6 addresses */
  int ServerDNSSearchDomains; /**< Boolean: If set, we don't force exit
                      * addresses to be FQDNs, but rather search for them in
                      * the local domains. */
  int ServerDNSDetectHijacking; /**< Boolean: If true, check for DNS failure
                                 * hijacking. */
  int ServerDNSRandomizeCase; /**< Boolean: Use the 0x20-hack to prevent
                               * DNS poisoning attacks. */
  char *ServerDNSResolvConfFile; /**< If provided, we configure our internal
                     * resolver from the file here rather than from
                     * /etc/resolv.conf (Unix) or the registry (Windows). */
  char *DirPortFrontPage; /**< This is a full path to a file with an html
                    disclaimer. This allows a server administrator to show
                    that they're running Tor and anyone visiting their server
                    will know this without any specialized knowledge. */
  int DisableDebuggerAttachment; /**< Currently Linux only specific attempt to
                                      disable ptrace; needs BSD testing. */
  /** Boolean: if set, we start even if our resolv.conf file is missing
   * or broken. */
  int ServerDNSAllowBrokenConfig;
  /** Boolean: if set, then even connections to private addresses will get
   * rate-limited. */
  int CountPrivateBandwidth;
  smartlist_t *ServerDNSTestAddresses; /**< A list of addresses that definitely
                                        * should be resolvable. Used for
                                        * testing our DNS server. */
  int EnforceDistinctSubnets; /**< If true, don't allow multiple routers in the
                               * same network zone in the same circuit. */
  int PortForwarding; /**< If true, use NAT-PMP or UPnP to automatically
                       * forward the DirPort and ORPort on the NAT device */
  char *PortForwardingHelper; /** < Filename or full path of the port
                                  forwarding helper executable */
  int AllowNonRFC953Hostnames; /**< If true, we allow connections to hostnames
                                * with weird characters. */
  /** If true, we try resolving hostnames with weird characters. */
  int ServerDNSAllowNonRFC953Hostnames;

  /** If true, we try to download extra-info documents (and we serve them,
   * if we are a cache).  For authorities, this is always true. */
  int DownloadExtraInfo;

  /** If true, and we are acting as a relay, allow exit circuits even when
   * we are the first hop of a circuit. */
  int AllowSingleHopExits;
  /** If true, don't allow relays with AllowSingleHopExits=1 to be used in
   * circuits that we build. */
  int ExcludeSingleHopRelays;
  /** If true, and the controller tells us to use a one-hop circuit, and the
   * exit allows it, we use it. */
  int AllowSingleHopCircuits;

  /** If true, we convert "www.google.com.foo.exit" addresses on the
   * socks/trans/natd ports into "www.google.com" addresses that
   * exit from the node "foo". Disabled by default since attacking
   * websites and exit relays can use it to manipulate your path
   * selection. */
  int AllowDotExit;

  /** If true, we will warn if a user gives us only an IP address
   * instead of a hostname. */
  int WarnUnsafeSocks;

  /** If true, the user wants us to collect statistics on clients
   * requesting network statuses from us as directory. */
  int DirReqStatistics;

  /** If true, the user wants us to collect statistics on port usage. */
  int ExitPortStatistics;

  /** If true, the user wants us to collect connection statistics. */
  int ConnDirectionStatistics;

  /** If true, the user wants us to collect cell statistics. */
  int CellStatistics;

  /** If true, the user wants us to collect statistics as entry node. */
  int EntryStatistics;

  /** If true, include statistics file contents in extra-info documents. */
  int ExtraInfoStatistics;

  /** If true, do not believe anybody who tells us that a domain resolves
   * to an internal address, or that an internal address has a PTR mapping.
   * Helps avoid some cross-site attacks. */
  int ClientDNSRejectInternalAddresses;

  /** If true, do not accept any requests to connect to internal addresses
   * over randomly chosen exits. */
  int ClientRejectInternalAddresses;

  /** If true, clients may connect over IPv6. XXX we don't really
      enforce this -- clients _may_ set up outgoing IPv6 connections
      even when this option is not set. */
  int ClientUseIPv6;
  /** If true, prefer an IPv6 OR port over an IPv4 one. */
  int ClientPreferIPv6ORPort;

  /** The length of time that we think a consensus should be fresh. */
  int V3AuthVotingInterval;
  /** The length of time we think it will take to distribute votes. */
  int V3AuthVoteDelay;
  /** The length of time we think it will take to distribute signatures. */
  int V3AuthDistDelay;
  /** The number of intervals we think a consensus should be valid. */
  int V3AuthNIntervalsValid;

  /** Should advertise and sign consensuses with a legacy key, for key
   * migration purposes? */
  int V3AuthUseLegacyKey;

  /** Location of bandwidth measurement file */
  char *V3BandwidthsFile;

  /** Authority only: key=value pairs that we add to our networkstatus
   * consensus vote on the 'params' line. */
  char *ConsensusParams;

  /** Authority only: minimum number of measured bandwidths we must see
   * before we only beliee measured bandwidths to assign flags. */
  int MinMeasuredBWsForAuthToIgnoreAdvertised;

  /** The length of time that we think an initial consensus should be fresh.
   * Only altered on testing networks. */
  int TestingV3AuthInitialVotingInterval;

  /** The length of time we think it will take to distribute initial votes.
   * Only altered on testing networks. */
  int TestingV3AuthInitialVoteDelay;

  /** The length of time we think it will take to distribute initial
   * signatures.  Only altered on testing networks.*/
  int TestingV3AuthInitialDistDelay;

  /** Offset in seconds added to the starting time for consensus
      voting. Only altered on testing networks. */
  int TestingV3AuthVotingStartOffset;

  /** If an authority has been around for less than this amount of time, it
   * does not believe its reachability information is accurate.  Only
   * altered on testing networks. */
  int TestingAuthDirTimeToLearnReachability;

  /** Clients don't download any descriptor this recent, since it will
   * probably not have propagated to enough caches.  Only altered on testing
   * networks. */
  int TestingEstimatedDescriptorPropagationTime;

  /** Schedule for when servers should download things in general.  Only
   * altered on testing networks. */
  smartlist_t *TestingServerDownloadSchedule;

  /** Schedule for when clients should download things in general.  Only
   * altered on testing networks. */
  smartlist_t *TestingClientDownloadSchedule;

  /** Schedule for when servers should download consensuses.  Only altered
   * on testing networks. */
  smartlist_t *TestingServerConsensusDownloadSchedule;

  /** Schedule for when clients should download consensuses.  Only altered
   * on testing networks. */
  smartlist_t *TestingClientConsensusDownloadSchedule;

  /** Schedule for when clients should download bridge descriptors.  Only
   * altered on testing networks. */
  smartlist_t *TestingBridgeDownloadSchedule;

  /** When directory clients have only a few descriptors to request, they
   * batch them until they have more, or until this amount of time has
   * passed.  Only altered on testing networks. */
  int TestingClientMaxIntervalWithoutRequest;

  /** How long do we let a directory connection stall before expiring
   * it?  Only altered on testing networks. */
  int TestingDirConnectionMaxStall;

  /** How many times will we try to fetch a consensus before we give
   * up?  Only altered on testing networks. */
  int TestingConsensusMaxDownloadTries;

  /** How many times will we try to download a router's descriptor before
   * giving up?  Only altered on testing networks. */
  int TestingDescriptorMaxDownloadTries;

  /** How many times will we try to download a microdescriptor before
   * giving up?  Only altered on testing networks. */
  int TestingMicrodescMaxDownloadTries;

  /** How many times will we try to fetch a certificate before giving
   * up?  Only altered on testing networks. */
  int TestingCertMaxDownloadTries;

  /** If true, we take part in a testing network. Change the defaults of a
   * couple of other configuration options and allow to change the values
   * of certain configuration options. */
  int TestingTorNetwork;

  /** Minimum value for the Exit flag threshold on testing networks. */
  uint64_t TestingMinExitFlagThreshold;

  /** Minimum value for the Fast flag threshold on testing networks. */
  uint64_t TestingMinFastFlagThreshold;

  /** Relays in a testing network which should be voted Guard
   * regardless of uptime and bandwidth. */
  routerset_t *TestingDirAuthVoteGuard;

  /** Enable CONN_BW events.  Only altered on testing networks. */
  int TestingEnableConnBwEvent;

  /** Enable CELL_STATS events.  Only altered on testing networks. */
  int TestingEnableCellStatsEvent;

  /** Enable TB_EMPTY events.  Only altered on testing networks. */
  int TestingEnableTbEmptyEvent;

  /** If true, and we have GeoIP data, and we're a bridge, keep a per-country
   * count of how many client addresses have contacted us so that we can help
   * the bridge authority guess which countries have blocked access to us. */
  int BridgeRecordUsageByCountry;

  /** Optionally, IPv4 and IPv6 GeoIP data. */
  char *GeoIPFile;
  char *GeoIPv6File;

  /** Autobool: if auto, then any attempt to Exclude{Exit,}Nodes a particular
   * country code will exclude all nodes in ?? and A1.  If true, all nodes in
   * ?? and A1 are excluded. Has no effect if we don't know any GeoIP data. */
  int GeoIPExcludeUnknown;

  /** If true, SIGHUP should reload the torrc.  Sometimes controllers want
   * to make this false. */
  int ReloadTorrcOnSIGHUP;

  /* The main parameter for picking circuits within a connection.
   *
   * If this value is positive, when picking a cell to relay on a connection,
   * we always relay from the circuit whose weighted cell count is lowest.
   * Cells are weighted exponentially such that if one cell is sent
   * 'CircuitPriorityHalflife' seconds before another, it counts for half as
   * much.
   *
   * If this value is zero, we're disabling the cell-EWMA algorithm.
   *
   * If this value is negative, we're using the default approach
   * according to either Tor or a parameter set in the consensus.
   */
  double CircuitPriorityHalflife;

  /** If true, do not enable IOCP on windows with bufferevents, even if
   * we think we could. */
  int DisableIOCP;
  /** For testing only: will go away eventually. */
  int UseFilteringSSLBufferevents;

  /** Set to true if the TestingTorNetwork configuration option is set.
   * This is used so that options_validate() has a chance to realize that
   * the defaults have changed. */
  int UsingTestNetworkDefaults_;

  /** If 1, we try to use microdescriptors to build circuits.  If 0, we don't.
   * If -1, Tor decides. */
  int UseMicrodescriptors;

  /** File where we should write the ControlPort. */
  char *ControlPortWriteToFile;
  /** Should that file be group-readable? */
  int ControlPortFileGroupReadable;

#define MAX_MAX_CLIENT_CIRCUITS_PENDING 1024
  /** Maximum number of non-open general-purpose origin circuits to allow at
   * once. */
  int MaxClientCircuitsPending;

  /** If 1, we always send optimistic data when it's supported.  If 0, we
   * never use it.  If -1, we do what the consensus says. */
  int OptimisticData;

  /** If 1, and we are using IOCP, we set the kernel socket SNDBUF and RCVBUF
   * to 0 to try to save kernel memory and avoid the dread "Out of buffers"
   * issue. */
  int UserspaceIOCPBuffers;

  /** If 1, we accept and launch no external network connections, except on
   * control ports. */
  int DisableNetwork;

  /**
   * Parameters for path-bias detection.
   * @{
   * These options override the default behavior of Tor's (**currently
   * experimental**) path bias detection algorithm. To try to find broken or
   * misbehaving guard nodes, Tor looks for nodes where more than a certain
   * fraction of circuits through that guard fail to get built.
   *
   * The PathBiasCircThreshold option controls how many circuits we need to
   * build through a guard before we make these checks.  The
   * PathBiasNoticeRate, PathBiasWarnRate and PathBiasExtremeRate options
   * control what fraction of circuits must succeed through a guard so we
   * won't write log messages.  If less than PathBiasExtremeRate circuits
   * succeed *and* PathBiasDropGuards is set to 1, we disable use of that
   * guard.
   *
   * When we have seen more than PathBiasScaleThreshold circuits through a
   * guard, we scale our observations by 0.5 (governed by the consensus) so
   * that new observations don't get swamped by old ones.
   *
   * By default, or if a negative value is provided for one of these options,
   * Tor uses reasonable defaults from the networkstatus consensus document.
   * If no defaults are available there, these options default to 150, .70,
   * .50, .30, 0, and 300 respectively.
   */
  int PathBiasCircThreshold;
  double PathBiasNoticeRate;
  double PathBiasWarnRate;
  double PathBiasExtremeRate;
  int PathBiasDropGuards;
  int PathBiasScaleThreshold;
  /** @} */

  /**
   * Parameters for path-bias use detection
   * @{
   * Similar to the above options, these options override the default behavior
   * of Tor's (**currently experimental**) path use bias detection algorithm.
   *
   * Where as the path bias parameters govern thresholds for successfully
   * building circuits, these four path use bias parameters govern thresholds
   * only for circuit usage. Circuits which receive no stream usage are not
   * counted by this detection algorithm. A used circuit is considered
   * successful if it is capable of carrying streams or otherwise receiving
   * well-formed responses to RELAY cells.
   *
   * By default, or if a negative value is provided for one of these options,
   * Tor uses reasonable defaults from the networkstatus consensus document.
   * If no defaults are available there, these options default to 20, .80,
   * .60, and 100, respectively.
   */
  int PathBiasUseThreshold;
  double PathBiasNoticeUseRate;
  double PathBiasExtremeUseRate;
  int PathBiasScaleUseThreshold;
  /** @} */

  int IPv6Exit; /**< Do we support exiting to IPv6 addresses? */

  char *TLSECGroup; /**< One of "P256", "P224", or nil for auto */

  /** Autobool: should we use the ntor handshake if we can? */
  int UseNTorHandshake;

  /** Fraction: */
  double PathsNeededToBuildCircuits;

  /** What expiry time shall we place on our SSL certs? "0" means we
   * should guess a suitable value. */
  int SSLKeyLifetime;

  /** How long (seconds) do we keep a guard before picking a new one? */
  int GuardLifetime;

  /** Should we send the timestamps that pre-023 hidden services want? */
  int Support022HiddenServices;
} or_options_t;

/** Persistent state for an onion router, as saved to disk. */
typedef struct {
  uint32_t magic_;
  /** The time at which we next plan to write the state to the disk.  Equal to
   * TIME_MAX if there are no savable changes, 0 if there are changes that
   * should be saved right away. */
  time_t next_write;

  /** When was the state last written to disk? */
  time_t LastWritten;

  /** Fields for accounting bandwidth use. */
  time_t AccountingIntervalStart;
  uint64_t AccountingBytesReadInInterval;
  uint64_t AccountingBytesWrittenInInterval;
  int AccountingSecondsActive;
  int AccountingSecondsToReachSoftLimit;
  time_t AccountingSoftLimitHitAt;
  uint64_t AccountingBytesAtSoftLimit;
  uint64_t AccountingExpectedUsage;

  /** A list of Entry Guard-related configuration lines. */
  config_line_t *EntryGuards;

  config_line_t *TransportProxies;

  /** These fields hold information on the history of bandwidth usage for
   * servers.  The "Ends" fields hold the time when we last updated the
   * bandwidth usage. The "Interval" fields hold the granularity, in seconds,
   * of the entries of Values.  The "Values" lists hold decimal string
   * representations of the number of bytes read or written in each
   * interval. The "Maxima" list holds decimal strings describing the highest
   * rate achieved during the interval.
   */
  time_t      BWHistoryReadEnds;
  int         BWHistoryReadInterval;
  smartlist_t *BWHistoryReadValues;
  smartlist_t *BWHistoryReadMaxima;
  time_t      BWHistoryWriteEnds;
  int         BWHistoryWriteInterval;
  smartlist_t *BWHistoryWriteValues;
  smartlist_t *BWHistoryWriteMaxima;
  time_t      BWHistoryDirReadEnds;
  int         BWHistoryDirReadInterval;
  smartlist_t *BWHistoryDirReadValues;
  smartlist_t *BWHistoryDirReadMaxima;
  time_t      BWHistoryDirWriteEnds;
  int         BWHistoryDirWriteInterval;
  smartlist_t *BWHistoryDirWriteValues;
  smartlist_t *BWHistoryDirWriteMaxima;

  /** Build time histogram */
  config_line_t * BuildtimeHistogram;
  unsigned int TotalBuildTimes;
  unsigned int CircuitBuildAbandonedCount;

  /** What version of Tor wrote this state file? */
  char *TorVersion;

  /** Holds any unrecognized values we found in the state file, in the order
   * in which we found them. */
  config_line_t *ExtraLines;

  /** When did we last rotate our onion key?  "0" for 'no idea'. */
  time_t LastRotatedOnionKey;
} or_state_t;

/** Change the next_write time of <b>state</b> to <b>when</b>, unless the
 * state is already scheduled to be written to disk earlier than <b>when</b>.
 */
static INLINE void or_state_mark_dirty(or_state_t *state, time_t when)
{
  if (state->next_write > when)
    state->next_write = when;
}

#define MAX_SOCKS_REPLY_LEN 1024
#define MAX_SOCKS_ADDR_LEN 256
#define SOCKS_NO_AUTH 0x00
#define SOCKS_USER_PASS 0x02

/** Please open a TCP connection to this addr:port. */
#define SOCKS_COMMAND_CONNECT       0x01
/** Please turn this FQDN into an IP address, privately. */
#define SOCKS_COMMAND_RESOLVE       0xF0
/** Please turn this IP address into an FQDN, privately. */
#define SOCKS_COMMAND_RESOLVE_PTR   0xF1

#define SOCKS_COMMAND_IS_CONNECT(c) ((c)==SOCKS_COMMAND_CONNECT)
#define SOCKS_COMMAND_IS_RESOLVE(c) ((c)==SOCKS_COMMAND_RESOLVE || \
                                     (c)==SOCKS_COMMAND_RESOLVE_PTR)

/** State of a SOCKS request from a user to an OP.  Also used to encode other
 * information for non-socks user request (such as those on TransPort and
 * DNSPort) */
struct socks_request_t {
  /** Which version of SOCKS did the client use? One of "0, 4, 5" -- where
   * 0 means that no socks handshake ever took place, and this is just a
   * stub connection (e.g. see connection_ap_make_link()). */
  uint8_t socks_version;
  /** If using socks5 authentication, which authentication type did we
   * negotiate?  currently we support 0 (no authentication) and 2
   * (username/password). */
  uint8_t auth_type;
  /** What is this stream's goal? One of the SOCKS_COMMAND_* values */
  uint8_t command;
  /** Which kind of listener created this stream? */
  uint8_t listener_type;
  size_t replylen; /**< Length of <b>reply</b>. */
  uint8_t reply[MAX_SOCKS_REPLY_LEN]; /**< Write an entry into this string if
                                    * we want to specify our own socks reply,
                                    * rather than using the default socks4 or
                                    * socks5 socks reply. We use this for the
                                    * two-stage socks5 handshake.
                                    */
  char address[MAX_SOCKS_ADDR_LEN]; /**< What address did the client ask to
                                       connect to/resolve? */
  uint16_t port; /**< What port did the client ask to connect to? */
  unsigned int has_finished : 1; /**< Has the SOCKS handshake finished? Used to
                              * make sure we send back a socks reply for
                              * every connection. */
  unsigned int got_auth : 1; /**< Have we received any authentication data? */
  /** If this is set, we will choose "no authentication" instead of
   * "username/password" authentication if both are offered. Used as input to
   * parse_socks. */
  unsigned int socks_prefer_no_auth : 1;

  /** Number of bytes in username; 0 if username is NULL */
  size_t usernamelen;
  /** Number of bytes in password; 0 if password is NULL */
  uint8_t passwordlen;
  /** The negotiated username value if any (for socks5), or the entire
   * authentication string (for socks4).  This value is NOT nul-terminated;
   * see usernamelen for its length. */
  char *username;
  /** The negotiated password value if any (for socks5). This value is NOT
   * nul-terminated; see passwordlen for its length. */
  char *password;
};

/********************************* circuitbuild.c **********************/

/** How many hops does a general-purpose circuit have by default? */
#define DEFAULT_ROUTE_LEN 3

/* Circuit Build Timeout "public" structures. */

/** Precision multiplier for the Bw weights */
#define BW_WEIGHT_SCALE   10000
#define BW_MIN_WEIGHT_SCALE 1
#define BW_MAX_WEIGHT_SCALE INT32_MAX

/** Total size of the circuit timeout history to accumulate.
 * 1000 is approx 2.5 days worth of continual-use circuits. */
#define CBT_NCIRCUITS_TO_OBSERVE 1000

/** Width of the histogram bins in milliseconds */
#define CBT_BIN_WIDTH ((build_time_t)50)

/** Number of modes to use in the weighted-avg computation of Xm */
#define CBT_DEFAULT_NUM_XM_MODES 3
#define CBT_MIN_NUM_XM_MODES 1
#define CBT_MAX_NUM_XM_MODES 20

/** A build_time_t is milliseconds */
typedef uint32_t build_time_t;

/**
 * CBT_BUILD_ABANDONED is our flag value to represent a force-closed
 * circuit (Aka a 'right-censored' pareto value).
 */
#define CBT_BUILD_ABANDONED ((build_time_t)(INT32_MAX-1))
#define CBT_BUILD_TIME_MAX ((build_time_t)(INT32_MAX))

/** Save state every 10 circuits */
#define CBT_SAVE_STATE_EVERY 10

/* Circuit build times consensus parameters */

/**
 * How long to wait before actually closing circuits that take too long to
 * build in terms of CDF quantile.
 */
#define CBT_DEFAULT_CLOSE_QUANTILE 95
#define CBT_MIN_CLOSE_QUANTILE CBT_MIN_QUANTILE_CUTOFF
#define CBT_MAX_CLOSE_QUANTILE CBT_MAX_QUANTILE_CUTOFF

/**
 * How many circuits count as recent when considering if the
 * connection has gone gimpy or changed.
 */
#define CBT_DEFAULT_RECENT_CIRCUITS 20
#define CBT_MIN_RECENT_CIRCUITS 3
#define CBT_MAX_RECENT_CIRCUITS 1000

/**
 * Maximum count of timeouts that finish the first hop in the past
 * RECENT_CIRCUITS before calculating a new timeout.
 *
 * This tells us whether to abandon timeout history and set
 * the timeout back to whatever circuit_build_times_get_initial_timeout()
 * gives us.
 */
#define CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT (CBT_DEFAULT_RECENT_CIRCUITS*9/10)
#define CBT_MIN_MAX_RECENT_TIMEOUT_COUNT 3
#define CBT_MAX_MAX_RECENT_TIMEOUT_COUNT 10000

/** Minimum circuits before estimating a timeout */
#define CBT_DEFAULT_MIN_CIRCUITS_TO_OBSERVE 100
#define CBT_MIN_MIN_CIRCUITS_TO_OBSERVE 1
#define CBT_MAX_MIN_CIRCUITS_TO_OBSERVE 10000

/** Cutoff percentile on the CDF for our timeout estimation. */
#define CBT_DEFAULT_QUANTILE_CUTOFF 80
#define CBT_MIN_QUANTILE_CUTOFF 10
#define CBT_MAX_QUANTILE_CUTOFF 99
double circuit_build_times_quantile_cutoff(void);

/** How often in seconds should we build a test circuit */
#define CBT_DEFAULT_TEST_FREQUENCY 60
#define CBT_MIN_TEST_FREQUENCY 1
#define CBT_MAX_TEST_FREQUENCY INT32_MAX

/** Lowest allowable value for CircuitBuildTimeout in milliseconds */
#define CBT_DEFAULT_TIMEOUT_MIN_VALUE (1500)
#define CBT_MIN_TIMEOUT_MIN_VALUE 500
#define CBT_MAX_TIMEOUT_MIN_VALUE INT32_MAX

/** Initial circuit build timeout in milliseconds */
#define CBT_DEFAULT_TIMEOUT_INITIAL_VALUE (60*1000)
#define CBT_MIN_TIMEOUT_INITIAL_VALUE CBT_MIN_TIMEOUT_MIN_VALUE
#define CBT_MAX_TIMEOUT_INITIAL_VALUE INT32_MAX
int32_t circuit_build_times_initial_timeout(void);

#if CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT < CBT_MIN_MAX_RECENT_TIMEOUT_COUNT
#error "RECENT_CIRCUITS is set too low."
#endif

/** Information about the state of our local network connection */
typedef struct {
  /** The timestamp we last completed a TLS handshake or received a cell */
  time_t network_last_live;
  /** If the network is not live, how many timeouts has this caused? */
  int nonlive_timeouts;
  /** Circular array of circuits that have made it to the first hop. Slot is
   * 1 if circuit timed out, 0 if circuit succeeded */
  int8_t *timeouts_after_firsthop;
  /** Number of elements allocated for the above array */
  int num_recent_circs;
  /** Index into circular array. */
  int after_firsthop_idx;
} network_liveness_t;

typedef struct circuit_build_times_s circuit_build_times_t;

/********************************* config.c ***************************/

/** An error from options_trial_assign() or options_init_from_string(). */
typedef enum setopt_err_t {
  SETOPT_OK = 0,
  SETOPT_ERR_MISC = -1,
  SETOPT_ERR_PARSE = -2,
  SETOPT_ERR_TRANSITION = -3,
  SETOPT_ERR_SETTING = -4,
} setopt_err_t;

/********************************* connection_edge.c *************************/

/** Enumerates possible origins of a client-side address mapping. */
typedef enum {
  /** We're remapping this address because the controller told us to. */
  ADDRMAPSRC_CONTROLLER,
  /** We're remapping this address because of an AutomapHostsOnResolve
   * configuration. */
  ADDRMAPSRC_AUTOMAP,
  /** We're remapping this address because our configuration (via torrc, the
   * command line, or a SETCONF command) told us to. */
  ADDRMAPSRC_TORRC,
  /** We're remapping this address because we have TrackHostExit configured,
   * and we want to remember to use the same exit next time. */
  ADDRMAPSRC_TRACKEXIT,
  /** We're remapping this address because we got a DNS resolution from a
   * Tor server that told us what its value was. */
  ADDRMAPSRC_DNS,

  /** No remapping has occurred.  This isn't a possible value for an
   * addrmap_entry_t; it's used as a null value when we need to answer "Why
   * did this remapping happen." */
  ADDRMAPSRC_NONE
} addressmap_entry_source_t;
#define addressmap_entry_source_bitfield_t ENUM_BF(addressmap_entry_source_t)

/********************************* control.c ***************************/

/** Used to indicate the type of a circuit event passed to the controller.
 * The various types are defined in control-spec.txt */
typedef enum circuit_status_event_t {
  CIRC_EVENT_LAUNCHED = 0,
  CIRC_EVENT_BUILT    = 1,
  CIRC_EVENT_EXTENDED = 2,
  CIRC_EVENT_FAILED   = 3,
  CIRC_EVENT_CLOSED   = 4,
} circuit_status_event_t;

/** Used to indicate the type of a CIRC_MINOR event passed to the controller.
 * The various types are defined in control-spec.txt . */
typedef enum circuit_status_minor_event_t {
  CIRC_MINOR_EVENT_PURPOSE_CHANGED,
  CIRC_MINOR_EVENT_CANNIBALIZED,
} circuit_status_minor_event_t;

/** Used to indicate the type of a stream event passed to the controller.
 * The various types are defined in control-spec.txt */
typedef enum stream_status_event_t {
  STREAM_EVENT_SENT_CONNECT = 0,
  STREAM_EVENT_SENT_RESOLVE = 1,
  STREAM_EVENT_SUCCEEDED    = 2,
  STREAM_EVENT_FAILED       = 3,
  STREAM_EVENT_CLOSED       = 4,
  STREAM_EVENT_NEW          = 5,
  STREAM_EVENT_NEW_RESOLVE  = 6,
  STREAM_EVENT_FAILED_RETRIABLE = 7,
  STREAM_EVENT_REMAP        = 8
} stream_status_event_t;

/** Used to indicate the type of an OR connection event passed to the
 * controller.  The various types are defined in control-spec.txt */
typedef enum or_conn_status_event_t {
  OR_CONN_EVENT_LAUNCHED     = 0,
  OR_CONN_EVENT_CONNECTED    = 1,
  OR_CONN_EVENT_FAILED       = 2,
  OR_CONN_EVENT_CLOSED       = 3,
  OR_CONN_EVENT_NEW          = 4,
} or_conn_status_event_t;

/** Used to indicate the type of a buildtime event */
typedef enum buildtimeout_set_event_t {
  BUILDTIMEOUT_SET_EVENT_COMPUTED  = 0,
  BUILDTIMEOUT_SET_EVENT_RESET     = 1,
  BUILDTIMEOUT_SET_EVENT_SUSPENDED = 2,
  BUILDTIMEOUT_SET_EVENT_DISCARD = 3,
  BUILDTIMEOUT_SET_EVENT_RESUME = 4
} buildtimeout_set_event_t;

/** Execute the statement <b>stmt</b>, which may log events concerning the
 * connection <b>conn</b>.  To prevent infinite loops, disable log messages
 * being sent to controllers if <b>conn</b> is a control connection.
 *
 * Stmt must not contain any return or goto statements.
 */
#define CONN_LOG_PROTECT(conn, stmt)                                    \
  STMT_BEGIN                                                            \
    int _log_conn_is_control;                                           \
    tor_assert(conn);                                                   \
    _log_conn_is_control = (conn->type == CONN_TYPE_CONTROL);           \
    if (_log_conn_is_control)                                           \
      disable_control_logging();                                        \
  STMT_BEGIN stmt; STMT_END;                                            \
    if (_log_conn_is_control)                                           \
      enable_control_logging();                                         \
  STMT_END

/** Enum describing various stages of bootstrapping, for use with controller
 * bootstrap status events. The values range from 0 to 100. */
typedef enum {
  BOOTSTRAP_STATUS_UNDEF=-1,
  BOOTSTRAP_STATUS_STARTING=0,
  BOOTSTRAP_STATUS_CONN_DIR=5,
  BOOTSTRAP_STATUS_HANDSHAKE=-2,
  BOOTSTRAP_STATUS_HANDSHAKE_DIR=10,
  BOOTSTRAP_STATUS_ONEHOP_CREATE=15,
  BOOTSTRAP_STATUS_REQUESTING_STATUS=20,
  BOOTSTRAP_STATUS_LOADING_STATUS=25,
  BOOTSTRAP_STATUS_LOADING_KEYS=40,
  BOOTSTRAP_STATUS_REQUESTING_DESCRIPTORS=45,
  BOOTSTRAP_STATUS_LOADING_DESCRIPTORS=50,
  BOOTSTRAP_STATUS_CONN_OR=80,
  BOOTSTRAP_STATUS_HANDSHAKE_OR=85,
  BOOTSTRAP_STATUS_CIRCUIT_CREATE=90,
  BOOTSTRAP_STATUS_DONE=100
} bootstrap_status_t;

/********************************* directory.c ***************************/

/** A pair of digests created by dir_split_resource_info_fingerprint_pairs() */
typedef struct {
  char first[DIGEST_LEN];
  char second[DIGEST_LEN];
} fp_pair_t;

/********************************* dirserv.c ***************************/

/** An enum to describe what format we're generating a routerstatus line in.
 */
typedef enum {
  /** For use in a v2 opinion */
  NS_V2,
  /** For use in a consensus networkstatus document (ns flavor) */
  NS_V3_CONSENSUS,
  /** For use in a vote networkstatus document */
  NS_V3_VOTE,
  /** For passing to the controlport in response to a GETINFO request */
  NS_CONTROL_PORT,
  /** For use in a consensus networkstatus document (microdesc flavor) */
  NS_V3_CONSENSUS_MICRODESC
} routerstatus_format_type_t;

#ifdef DIRSERV_PRIVATE
typedef struct measured_bw_line_t {
  char node_id[DIGEST_LEN];
  char node_hex[MAX_HEX_NICKNAME_LEN+1];
  long int bw_kb;
} measured_bw_line_t;

#endif

/********************************* dirvote.c ************************/

/** Describes the schedule by which votes should be generated. */
typedef struct vote_timing_t {
  /** Length in seconds between one consensus becoming valid and the next
   * becoming valid. */
  int vote_interval;
  /** For how many intervals is a consensus valid? */
  int n_intervals_valid;
  /** Time in seconds allowed to propagate votes */
  int vote_delay;
  /** Time in seconds allowed to propagate signatures */
  int dist_delay;
} vote_timing_t;

/********************************* geoip.c **************************/

/** Indicates an action that we might be noting geoip statistics on.
 * Note that if we're noticing CONNECT, we're a bridge, and if we're noticing
 * the others, we're not.
 */
typedef enum {
  /** We've noticed a connection as a bridge relay or entry guard. */
  GEOIP_CLIENT_CONNECT = 0,
  /** We've served a networkstatus consensus as a directory server. */
  GEOIP_CLIENT_NETWORKSTATUS = 1,
} geoip_client_action_t;
/** Indicates either a positive reply or a reason for rejectng a network
 * status request that will be included in geoip statistics. */
typedef enum {
  /** Request is answered successfully. */
  GEOIP_SUCCESS = 0,
  /** V3 network status is not signed by a sufficient number of requested
   * authorities. */
  GEOIP_REJECT_NOT_ENOUGH_SIGS = 1,
  /** Requested network status object is unavailable. */
  GEOIP_REJECT_UNAVAILABLE = 2,
  /** Requested network status not found. */
  GEOIP_REJECT_NOT_FOUND = 3,
  /** Network status has not been modified since If-Modified-Since time. */
  GEOIP_REJECT_NOT_MODIFIED = 4,
  /** Directory is busy. */
  GEOIP_REJECT_BUSY = 5,
} geoip_ns_response_t;
#define GEOIP_NS_RESPONSE_NUM 6

/** Directory requests that we are measuring can be either direct or
 * tunneled. */
typedef enum {
  DIRREQ_DIRECT = 0,
  DIRREQ_TUNNELED = 1,
} dirreq_type_t;

/** Possible states for either direct or tunneled directory requests that
 * are relevant for determining network status download times. */
typedef enum {
  /** Found that the client requests a network status; applies to both
   * direct and tunneled requests; initial state of a request that we are
   * measuring. */
  DIRREQ_IS_FOR_NETWORK_STATUS = 0,
  /** Finished writing a network status to the directory connection;
   * applies to both direct and tunneled requests; completes a direct
   * request. */
  DIRREQ_FLUSHING_DIR_CONN_FINISHED = 1,
  /** END cell sent to circuit that initiated a tunneled request. */
  DIRREQ_END_CELL_SENT = 2,
  /** Flushed last cell from queue of the circuit that initiated a
    * tunneled request to the outbuf of the OR connection. */
  DIRREQ_CIRC_QUEUE_FLUSHED = 3,
  /** Flushed last byte from buffer of the channel belonging to the
    * circuit that initiated a tunneled request; completes a tunneled
    * request. */
  DIRREQ_CHANNEL_BUFFER_FLUSHED = 4
} dirreq_state_t;

#define WRITE_STATS_INTERVAL (24*60*60)

/********************************* microdesc.c *************************/

typedef struct microdesc_cache_t microdesc_cache_t;

/********************************* networkstatus.c *********************/

/** Possible statuses of a version of Tor, given opinions from the directory
 * servers. */
typedef enum version_status_t {
  VS_RECOMMENDED=0, /**< This version is listed as recommended. */
  VS_OLD=1, /**< This version is older than any recommended version. */
  VS_NEW=2, /**< This version is newer than any recommended version. */
  VS_NEW_IN_SERIES=3, /**< This version is newer than any recommended version
                       * in its series, but later recommended versions exist.
                       */
  VS_UNRECOMMENDED=4, /**< This version is not recommended (general case). */
  VS_EMPTY=5, /**< The version list was empty; no agreed-on versions. */
  VS_UNKNOWN, /**< We have no idea. */
} version_status_t;

/********************************* policies.c ************************/

/** Outcome of applying an address policy to an address. */
typedef enum {
  /** The address was accepted */
  ADDR_POLICY_ACCEPTED=0,
  /** The address was rejected */
  ADDR_POLICY_REJECTED=-1,
  /** Part of the address was unknown, but as far as we can tell, it was
   * accepted. */
  ADDR_POLICY_PROBABLY_ACCEPTED=1,
  /** Part of the address was unknown, but as far as we can tell, it was
   * rejected. */
  ADDR_POLICY_PROBABLY_REJECTED=2,
} addr_policy_result_t;

/********************************* rephist.c ***************************/

/** Possible public/private key operations in Tor: used to keep track of where
 * we're spending our time. */
typedef enum {
  SIGN_DIR, SIGN_RTR,
  VERIFY_DIR, VERIFY_RTR,
  ENC_ONIONSKIN, DEC_ONIONSKIN,
  TLS_HANDSHAKE_C, TLS_HANDSHAKE_S,
  REND_CLIENT, REND_MID, REND_SERVER,
} pk_op_t;

/********************************* rendcommon.c ***************************/

/** Hidden-service side configuration of client authorization. */
typedef struct rend_authorized_client_t {
  char *client_name;
  char descriptor_cookie[REND_DESC_COOKIE_LEN];
  crypto_pk_t *client_key;
} rend_authorized_client_t;

/** ASCII-encoded v2 hidden service descriptor. */
typedef struct rend_encoded_v2_service_descriptor_t {
  char desc_id[DIGEST_LEN]; /**< Descriptor ID. */
  char *desc_str; /**< Descriptor string. */
} rend_encoded_v2_service_descriptor_t;

/** The maximum number of non-circuit-build-timeout failures a hidden
 * service client will tolerate while trying to build a circuit to an
 * introduction point.  See also rend_intro_point_t.unreachable_count. */
#define MAX_INTRO_POINT_REACHABILITY_FAILURES 5

/** The maximum number of distinct INTRODUCE2 cells which a hidden
 * service's introduction point will receive before it begins to
 * expire.
 *
 * XXX023 Is this number at all sane? */
#define INTRO_POINT_LIFETIME_INTRODUCTIONS 16384

/** The minimum number of seconds that an introduction point will last
 * before expiring due to old age.  (If it receives
 * INTRO_POINT_LIFETIME_INTRODUCTIONS INTRODUCE2 cells, it may expire
 * sooner.)
 *
 * XXX023 Should this be configurable? */
#define INTRO_POINT_LIFETIME_MIN_SECONDS (18*60*60)
/** The maximum number of seconds that an introduction point will last
 * before expiring due to old age.
 *
 * XXX023 Should this be configurable? */
#define INTRO_POINT_LIFETIME_MAX_SECONDS (24*60*60)

/** Introduction point information.  Used both in rend_service_t (on
 * the service side) and in rend_service_descriptor_t (on both the
 * client and service side). */
typedef struct rend_intro_point_t {
  extend_info_t *extend_info; /**< Extend info of this introduction point. */
  crypto_pk_t *intro_key; /**< Introduction key that replaces the service
                               * key, if this descriptor is V2. */

  /** (Client side only) Flag indicating that a timeout has occurred
   * after sending an INTRODUCE cell to this intro point.  After a
   * timeout, an intro point should not be tried again during the same
   * hidden service connection attempt, but it may be tried again
   * during a future connection attempt. */
  unsigned int timed_out : 1;

  /** (Client side only) The number of times we have failed to build a
   * circuit to this intro point for some reason other than our
   * circuit-build timeout.  See also MAX_INTRO_POINT_REACHABILITY_FAILURES. */
  unsigned int unreachable_count : 3;

  /** (Service side only) Flag indicating that this intro point was
   * included in the last HS descriptor we generated. */
  unsigned int listed_in_last_desc : 1;

  /** (Service side only) Flag indicating that
   * rend_service_note_removing_intro_point has been called for this
   * intro point. */
  unsigned int rend_service_note_removing_intro_point_called : 1;

  /** (Service side only) A replay cache recording the RSA-encrypted parts
   * of INTRODUCE2 cells this intro point's circuit has received.  This is
   * used to prevent replay attacks. */
  replaycache_t *accepted_intro_rsa_parts;

  /** (Service side only) Count of INTRODUCE2 cells accepted from this
   * intro point.
   */
  int accepted_introduce2_count;

  /** (Service side only) The time at which this intro point was first
   * published, or -1 if this intro point has not yet been
   * published. */
  time_t time_published;

  /** (Service side only) The time at which this intro point should
   * (start to) expire, or -1 if we haven't decided when this intro
   * point should expire. */
  time_t time_to_expire;

  /** (Service side only) The time at which we decided that this intro
   * point should start expiring, or -1 if this intro point is not yet
   * expiring.
   *
   * This field also serves as a flag to indicate that we have decided
   * to expire this intro point, in case intro_point_should_expire_now
   * flaps (perhaps due to a clock jump; perhaps due to other
   * weirdness, or even a (present or future) bug). */
  time_t time_expiring;
} rend_intro_point_t;

#define REND_PROTOCOL_VERSION_BITMASK_WIDTH 16

/** Information used to connect to a hidden service.  Used on both the
 * service side and the client side. */
typedef struct rend_service_descriptor_t {
  crypto_pk_t *pk; /**< This service's public key. */
  int version; /**< Version of the descriptor format: 0 or 2. */
  time_t timestamp; /**< Time when the descriptor was generated. */
  /** Bitmask: which introduce/rendezvous protocols are supported?
   * (We allow bits '0', '1', '2' and '3' to be set.) */
  unsigned protocols : REND_PROTOCOL_VERSION_BITMASK_WIDTH;
  /** List of the service's introduction points.  Elements are removed if
   * introduction attempts fail. */
  smartlist_t *intro_nodes;
  /** Has descriptor been uploaded to all hidden service directories? */
  int all_uploads_performed;
  /** List of hidden service directories to which an upload request for
   * this descriptor could be sent. Smartlist exists only when at least one
   * of the previous upload requests failed (otherwise it's not important
   * to know which uploads succeeded and which not). */
  smartlist_t *successful_uploads;
} rend_service_descriptor_t;

/** A cached rendezvous descriptor. */
typedef struct rend_cache_entry_t {
  size_t len; /**< Length of <b>desc</b> */
  time_t received; /**< When was the descriptor received? */
  char *desc; /**< Service descriptor */
  rend_service_descriptor_t *parsed; /**< Parsed value of 'desc' */
} rend_cache_entry_t;

/********************************* routerlist.c ***************************/

/** Represents information about a single trusted or fallback directory
 * server. */
typedef struct dir_server_t {
  char *description;
  char *nickname;
  char *address; /**< Hostname. */
  uint32_t addr; /**< IPv4 address. */
  uint16_t dir_port; /**< Directory port. */
  uint16_t or_port; /**< OR port: Used for tunneling connections. */
  double weight; /** Weight used when selecting this node at random */
  char digest[DIGEST_LEN]; /**< Digest of identity key. */
  char v3_identity_digest[DIGEST_LEN]; /**< Digest of v3 (authority only,
                                        * high-security) identity key. */

  unsigned int is_running:1; /**< True iff we think this server is running. */
  unsigned int is_authority:1; /**< True iff this is a directory authority
                                * of some kind. */

  /** True iff this server has accepted the most recent server descriptor
   * we tried to upload to it. */
  unsigned int has_accepted_serverdesc:1;

  /** What kind of authority is this? (Bitfield.) */
  dirinfo_type_t type;

  time_t addr_current_at; /**< When was the document that we derived the
                           * address information from published? */

  routerstatus_t fake_status; /**< Used when we need to pass this trusted
                               * dir_server_t to directory_initiate_command_*
                               * as a routerstatus_t.  Not updated by the
                               * router-status management code!
                               **/
} dir_server_t;

#define ROUTER_REQUIRED_MIN_BANDWIDTH (20*1024)

#define ROUTER_MAX_DECLARED_BANDWIDTH INT32_MAX

/* Flags for pick_directory_server() and pick_trusteddirserver(). */
/** Flag to indicate that we should not automatically be willing to use
 * ourself to answer a directory request.
 * Passed to router_pick_directory_server (et al).*/
#define PDS_ALLOW_SELF                 (1<<0)
/** Flag to indicate that if no servers seem to be up, we should mark all
 * directory servers as up and try again.
 * Passed to router_pick_directory_server (et al).*/
#define PDS_RETRY_IF_NO_SERVERS        (1<<1)
/** Flag to indicate that we should not exclude directory servers that
 * our ReachableAddress settings would exclude.  This usually means that
 * we're going to connect to the server over Tor, and so we don't need to
 * worry about our firewall telling us we can't.
 * Passed to router_pick_directory_server (et al).*/
#define PDS_IGNORE_FASCISTFIREWALL     (1<<2)
/** Flag to indicate that we should not use any directory authority to which
 * we have an existing directory connection for downloading server descriptors
 * or extrainfo documents.
 *
 * Passed to router_pick_directory_server (et al)
 *
 * [XXXX NOTE: This option is only implemented for pick_trusteddirserver,
 *  not pick_directory_server.  If we make it work on pick_directory_server
 *  too, we could conservatively make it only prevent multiple fetches to
 *  the same authority, or we could aggressively make it prevent multiple
 *  fetches to _any_ single directory server.]
 */
#define PDS_NO_EXISTING_SERVERDESC_FETCH (1<<3)
#define PDS_NO_EXISTING_MICRODESC_FETCH (1<<4)

/** This node is to be chosen as a directory guard, so don't choose any
 * node that's currently a guard. */
#define PDS_FOR_GUARD (1<<5)

/** Possible ways to weight routers when choosing one randomly.  See
 * routerlist_sl_choose_by_bandwidth() for more information.*/
typedef enum bandwidth_weight_rule_t {
  NO_WEIGHTING, WEIGHT_FOR_EXIT, WEIGHT_FOR_MID, WEIGHT_FOR_GUARD,
  WEIGHT_FOR_DIR
} bandwidth_weight_rule_t;

/** Flags to be passed to control router_choose_random_node() to indicate what
 * kind of nodes to pick according to what algorithm. */
typedef enum {
  CRN_NEED_UPTIME = 1<<0,
  CRN_NEED_CAPACITY = 1<<1,
  CRN_NEED_GUARD = 1<<2,
  CRN_ALLOW_INVALID = 1<<3,
  /* XXXX not used, apparently. */
  CRN_WEIGHT_AS_EXIT = 1<<5,
  CRN_NEED_DESC = 1<<6
} router_crn_flags_t;

/** Return value for router_add_to_routerlist() and dirserv_add_descriptor() */
typedef enum was_router_added_t {
  ROUTER_ADDED_SUCCESSFULLY = 1,
  ROUTER_ADDED_NOTIFY_GENERATOR = 0,
  ROUTER_BAD_EI = -1,
  ROUTER_WAS_NOT_NEW = -2,
  ROUTER_NOT_IN_CONSENSUS = -3,
  ROUTER_NOT_IN_CONSENSUS_OR_NETWORKSTATUS = -4,
  ROUTER_AUTHDIR_REJECTS = -5,
  ROUTER_WAS_NOT_WANTED = -6
} was_router_added_t;

/********************************* routerparse.c ************************/

#define MAX_STATUS_TAG_LEN 32
/** Structure to hold parsed Tor versions.  This is a little messier
 * than we would like it to be, because we changed version schemes with 0.1.0.
 *
 * See version-spec.txt for the whole business.
 */
typedef struct tor_version_t {
  int major;
  int minor;
  int micro;
  /** Release status.  For version in the post-0.1 format, this is always
   * VER_RELEASE. */
  enum { VER_PRE=0, VER_RC=1, VER_RELEASE=2, } status;
  int patchlevel;
  char status_tag[MAX_STATUS_TAG_LEN];
  int svn_revision;

  int git_tag_len;
  char git_tag[DIGEST_LEN];
} tor_version_t;

#endif

