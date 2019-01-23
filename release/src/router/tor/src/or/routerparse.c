/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file routerparse.c
 * \brief Code to parse and validate router descriptors, consenus directories,
 *   and similar objects.
 *
 * The objects parsed by this module use a common text-based metaformat,
 * documented in dir-spec.txt in torspec.git.  This module is itself divided
 * into two major kinds of function: code to handle the metaformat, and code
 * to convert from particular instances of the metaformat into the
 * objects that Tor uses.
 *
 * The generic parsing code works by calling a table-based tokenizer on the
 * input string.  Each token corresponds to a single line with a token, plus
 * optional arguments on that line, plus an optional base-64 encoded object
 * after that line.  Each token has a definition in a table of token_rule_t
 * entries that describes how many arguments it can take, whether it takes an
 * object, how many times it may appear, whether it must appear first, and so
 * on.
 *
 * The tokenizer function tokenize_string() converts its string input into a
 * smartlist full of instances of directory_token_t, according to a provided
 * table of token_rule_t.
 *
 * The generic parts of this module additionally include functions for
 * finding the start and end of signed information inside a signed object, and
 * computing the digest that will be signed.
 *
 * There are also functions for saving objects to disk that have caused
 * parsing to fail.
 *
 * The specific parts of this module describe conversions between
 * particular lists of directory_token_t and particular objects.  The
 * kinds of objects that can be parsed here are:
 *  <ul>
 *  <li>router descriptors (managed from routerlist.c)
 *  <li>extra-info documents (managed from routerlist.c)
 *  <li>microdescriptors (managed from microdesc.c)
 *  <li>vote and consensus networkstatus documents, and the routerstatus_t
 *    objects that they comprise (managed from networkstatus.c)
 *  <li>detached-signature objects used by authorities for gathering
 *    signatures on the networkstatus consensus (managed from dirvote.c)
 *  <li>authority key certificates (managed from routerlist.c)
 *  <li>hidden service descriptors (managed from rendcommon.c and rendcache.c)
 * </ul>
 *
 * For no terribly good reason, the functions to <i>generate</i> signatures on
 * the above directory objects are also in this module.
 **/

#define ROUTERPARSE_PRIVATE

#include "or.h"
#include "config.h"
#include "circuitstats.h"
#include "dirserv.h"
#include "dirvote.h"
#include "policies.h"
#include "protover.h"
#include "rendcommon.h"
#include "router.h"
#include "routerlist.h"
#include "memarea.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "rephist.h"
#include "routerkeys.h"
#include "routerparse.h"
#include "entrynodes.h"
#include "torcert.h"
#include "sandbox.h"
#include "shared_random.h"

#undef log
#include <math.h>

/****************************************************************************/

/** Enumeration of possible token types.  The ones starting with K_ correspond
 * to directory 'keywords'. A_ is for an annotation, R or C is related to
 * hidden services, ERR_ is an error in the tokenizing process, EOF_ is an
 * end-of-file marker, and NIL_ is used to encode not-a-token.
 */
typedef enum {
  K_ACCEPT = 0,
  K_ACCEPT6,
  K_DIRECTORY_SIGNATURE,
  K_RECOMMENDED_SOFTWARE,
  K_REJECT,
  K_REJECT6,
  K_ROUTER,
  K_SIGNED_DIRECTORY,
  K_SIGNING_KEY,
  K_ONION_KEY,
  K_ONION_KEY_NTOR,
  K_ROUTER_SIGNATURE,
  K_PUBLISHED,
  K_RUNNING_ROUTERS,
  K_ROUTER_STATUS,
  K_PLATFORM,
  K_PROTO,
  K_OPT,
  K_BANDWIDTH,
  K_CONTACT,
  K_NETWORK_STATUS,
  K_UPTIME,
  K_DIR_SIGNING_KEY,
  K_FAMILY,
  K_FINGERPRINT,
  K_HIBERNATING,
  K_READ_HISTORY,
  K_WRITE_HISTORY,
  K_NETWORK_STATUS_VERSION,
  K_DIR_SOURCE,
  K_DIR_OPTIONS,
  K_CLIENT_VERSIONS,
  K_SERVER_VERSIONS,
  K_RECOMMENDED_CLIENT_PROTOCOLS,
  K_RECOMMENDED_RELAY_PROTOCOLS,
  K_REQUIRED_CLIENT_PROTOCOLS,
  K_REQUIRED_RELAY_PROTOCOLS,
  K_OR_ADDRESS,
  K_ID,
  K_P,
  K_P6,
  K_R,
  K_A,
  K_S,
  K_V,
  K_W,
  K_M,
  K_EXTRA_INFO,
  K_EXTRA_INFO_DIGEST,
  K_CACHES_EXTRA_INFO,
  K_HIDDEN_SERVICE_DIR,
  K_ALLOW_SINGLE_HOP_EXITS,
  K_IPV6_POLICY,
  K_ROUTER_SIG_ED25519,
  K_IDENTITY_ED25519,
  K_MASTER_KEY_ED25519,
  K_ONION_KEY_CROSSCERT,
  K_NTOR_ONION_KEY_CROSSCERT,

  K_DIRREQ_END,
  K_DIRREQ_V2_IPS,
  K_DIRREQ_V3_IPS,
  K_DIRREQ_V2_REQS,
  K_DIRREQ_V3_REQS,
  K_DIRREQ_V2_SHARE,
  K_DIRREQ_V3_SHARE,
  K_DIRREQ_V2_RESP,
  K_DIRREQ_V3_RESP,
  K_DIRREQ_V2_DIR,
  K_DIRREQ_V3_DIR,
  K_DIRREQ_V2_TUN,
  K_DIRREQ_V3_TUN,
  K_ENTRY_END,
  K_ENTRY_IPS,
  K_CELL_END,
  K_CELL_PROCESSED,
  K_CELL_QUEUED,
  K_CELL_TIME,
  K_CELL_CIRCS,
  K_EXIT_END,
  K_EXIT_WRITTEN,
  K_EXIT_READ,
  K_EXIT_OPENED,

  K_DIR_KEY_CERTIFICATE_VERSION,
  K_DIR_IDENTITY_KEY,
  K_DIR_KEY_PUBLISHED,
  K_DIR_KEY_EXPIRES,
  K_DIR_KEY_CERTIFICATION,
  K_DIR_KEY_CROSSCERT,
  K_DIR_ADDRESS,
  K_DIR_TUNNELLED,

  K_VOTE_STATUS,
  K_VALID_AFTER,
  K_FRESH_UNTIL,
  K_VALID_UNTIL,
  K_VOTING_DELAY,

  K_KNOWN_FLAGS,
  K_PARAMS,
  K_BW_WEIGHTS,
  K_VOTE_DIGEST,
  K_CONSENSUS_DIGEST,
  K_ADDITIONAL_DIGEST,
  K_ADDITIONAL_SIGNATURE,
  K_CONSENSUS_METHODS,
  K_CONSENSUS_METHOD,
  K_LEGACY_DIR_KEY,
  K_DIRECTORY_FOOTER,
  K_SIGNING_CERT_ED,
  K_SR_FLAG,
  K_COMMIT,
  K_PREVIOUS_SRV,
  K_CURRENT_SRV,
  K_PACKAGE,

  A_PURPOSE,
  A_LAST_LISTED,
  A_UNKNOWN_,

  R_RENDEZVOUS_SERVICE_DESCRIPTOR,
  R_VERSION,
  R_PERMANENT_KEY,
  R_SECRET_ID_PART,
  R_PUBLICATION_TIME,
  R_PROTOCOL_VERSIONS,
  R_INTRODUCTION_POINTS,
  R_SIGNATURE,

  R_IPO_IDENTIFIER,
  R_IPO_IP_ADDRESS,
  R_IPO_ONION_PORT,
  R_IPO_ONION_KEY,
  R_IPO_SERVICE_KEY,

  C_CLIENT_NAME,
  C_DESCRIPTOR_COOKIE,
  C_CLIENT_KEY,

  ERR_,
  EOF_,
  NIL_
} directory_keyword;

#define MIN_ANNOTATION A_PURPOSE
#define MAX_ANNOTATION A_UNKNOWN_

/** Structure to hold a single directory token.
 *
 * We parse a directory by breaking it into "tokens", each consisting
 * of a keyword, a line full of arguments, and a binary object.  The
 * arguments and object are both optional, depending on the keyword
 * type.
 *
 * This structure is only allocated in memareas; do not allocate it on
 * the heap, or token_clear() won't work.
 */
typedef struct directory_token_t {
  directory_keyword tp;        /**< Type of the token. */
  int n_args:30;               /**< Number of elements in args */
  char **args;                 /**< Array of arguments from keyword line. */

  char *object_type;           /**< -----BEGIN [object_type]-----*/
  size_t object_size;          /**< Bytes in object_body */
  char *object_body;           /**< Contents of object, base64-decoded. */

  crypto_pk_t *key;        /**< For public keys only.  Heap-allocated. */

  char *error;                 /**< For ERR_ tokens only. */
} directory_token_t;

/* ********************************************************************** */

/** We use a table of rules to decide how to parse each token type. */

/** Rules for whether the keyword needs an object. */
typedef enum {
  NO_OBJ,        /**< No object, ever. */
  NEED_OBJ,      /**< Object is required. */
  NEED_SKEY_1024,/**< Object is required, and must be a 1024 bit private key */
  NEED_KEY_1024, /**< Object is required, and must be a 1024 bit public key */
  NEED_KEY,      /**< Object is required, and must be a public key. */
  OBJ_OK,        /**< Object is optional. */
} obj_syntax;

#define AT_START 1
#define AT_END 2

/** Determines the parsing rules for a single token type. */
typedef struct token_rule_t {
  /** The string value of the keyword identifying the type of item. */
  const char *t;
  /** The corresponding directory_keyword enum. */
  directory_keyword v;
  /** Minimum number of arguments for this item */
  int min_args;
  /** Maximum number of arguments for this item */
  int max_args;
  /** If true, we concatenate all arguments for this item into a single
   * string. */
  int concat_args;
  /** Requirements on object syntax for this item. */
  obj_syntax os;
  /** Lowest number of times this item may appear in a document. */
  int min_cnt;
  /** Highest number of times this item may appear in a document. */
  int max_cnt;
  /** One or more of AT_START/AT_END to limit where the item may appear in a
   * document. */
  int pos;
  /** True iff this token is an annotation. */
  int is_annotation;
} token_rule_t;

/**
 * @name macros for defining token rules
 *
 * Helper macros to define token tables.  's' is a string, 't' is a
 * directory_keyword, 'a' is a trio of argument multiplicities, and 'o' is an
 * object syntax.
 */
/**@{*/

/** Appears to indicate the end of a table. */
#define END_OF_TABLE { NULL, NIL_, 0,0,0, NO_OBJ, 0, INT_MAX, 0, 0 }
/** An item with no restrictions: used for obsolete document types */
#define T(s,t,a,o)    { s, t, a, o, 0, INT_MAX, 0, 0 }
/** An item with no restrictions on multiplicity or location. */
#define T0N(s,t,a,o)  { s, t, a, o, 0, INT_MAX, 0, 0 }
/** An item that must appear exactly once */
#define T1(s,t,a,o)   { s, t, a, o, 1, 1, 0, 0 }
/** An item that must appear exactly once, at the start of the document */
#define T1_START(s,t,a,o)   { s, t, a, o, 1, 1, AT_START, 0 }
/** An item that must appear exactly once, at the end of the document */
#define T1_END(s,t,a,o)   { s, t, a, o, 1, 1, AT_END, 0 }
/** An item that must appear one or more times */
#define T1N(s,t,a,o)  { s, t, a, o, 1, INT_MAX, 0, 0 }
/** An item that must appear no more than once */
#define T01(s,t,a,o)  { s, t, a, o, 0, 1, 0, 0 }
/** An annotation that must appear no more than once */
#define A01(s,t,a,o)  { s, t, a, o, 0, 1, 0, 1 }

/** Argument multiplicity: any number of arguments. */
#define ARGS        0,INT_MAX,0
/** Argument multiplicity: no arguments. */
#define NO_ARGS     0,0,0
/** Argument multiplicity: concatenate all arguments. */
#define CONCAT_ARGS 1,1,1
/** Argument multiplicity: at least <b>n</b> arguments. */
#define GE(n)       n,INT_MAX,0
/** Argument multiplicity: exactly <b>n</b> arguments. */
#define EQ(n)       n,n,0
/**@}*/

/** List of tokens recognized in router descriptors */
static token_rule_t routerdesc_token_table[] = {
  T0N("reject",              K_REJECT,              ARGS,    NO_OBJ ),
  T0N("accept",              K_ACCEPT,              ARGS,    NO_OBJ ),
  T0N("reject6",             K_REJECT6,             ARGS,    NO_OBJ ),
  T0N("accept6",             K_ACCEPT6,             ARGS,    NO_OBJ ),
  T1_START( "router",        K_ROUTER,              GE(5),   NO_OBJ ),
  T01("ipv6-policy",         K_IPV6_POLICY,         CONCAT_ARGS, NO_OBJ),
  T1( "signing-key",         K_SIGNING_KEY,         NO_ARGS, NEED_KEY_1024 ),
  T1( "onion-key",           K_ONION_KEY,           NO_ARGS, NEED_KEY_1024 ),
  T01("ntor-onion-key",      K_ONION_KEY_NTOR,      GE(1), NO_OBJ ),
  T1_END( "router-signature",    K_ROUTER_SIGNATURE,    NO_ARGS, NEED_OBJ ),
  T1( "published",           K_PUBLISHED,       CONCAT_ARGS, NO_OBJ ),
  T01("uptime",              K_UPTIME,              GE(1),   NO_OBJ ),
  T01("fingerprint",         K_FINGERPRINT,     CONCAT_ARGS, NO_OBJ ),
  T01("hibernating",         K_HIBERNATING,         GE(1),   NO_OBJ ),
  T01("platform",            K_PLATFORM,        CONCAT_ARGS, NO_OBJ ),
  T01("proto",               K_PROTO,           CONCAT_ARGS, NO_OBJ ),
  T01("contact",             K_CONTACT,         CONCAT_ARGS, NO_OBJ ),
  T01("read-history",        K_READ_HISTORY,        ARGS,    NO_OBJ ),
  T01("write-history",       K_WRITE_HISTORY,       ARGS,    NO_OBJ ),
  T01("extra-info-digest",   K_EXTRA_INFO_DIGEST,   GE(1),   NO_OBJ ),
  T01("hidden-service-dir",  K_HIDDEN_SERVICE_DIR,  NO_ARGS, NO_OBJ ),
  T01("identity-ed25519",    K_IDENTITY_ED25519,    NO_ARGS, NEED_OBJ ),
  T01("master-key-ed25519",  K_MASTER_KEY_ED25519,  GE(1),   NO_OBJ ),
  T01("router-sig-ed25519",  K_ROUTER_SIG_ED25519,  GE(1),   NO_OBJ ),
  T01("onion-key-crosscert", K_ONION_KEY_CROSSCERT, NO_ARGS, NEED_OBJ ),
  T01("ntor-onion-key-crosscert", K_NTOR_ONION_KEY_CROSSCERT,
                                                    EQ(1),   NEED_OBJ ),

  T01("allow-single-hop-exits",K_ALLOW_SINGLE_HOP_EXITS,    NO_ARGS, NO_OBJ ),

  T01("family",              K_FAMILY,              ARGS,    NO_OBJ ),
  T01("caches-extra-info",   K_CACHES_EXTRA_INFO,   NO_ARGS, NO_OBJ ),
  T0N("or-address",          K_OR_ADDRESS,          GE(1),   NO_OBJ ),

  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),
  T1( "bandwidth",           K_BANDWIDTH,           GE(3),   NO_OBJ ),
  A01("@purpose",            A_PURPOSE,             GE(1),   NO_OBJ ),
  T01("tunnelled-dir-server",K_DIR_TUNNELLED,       NO_ARGS, NO_OBJ ),

  END_OF_TABLE
};

/** List of tokens recognized in extra-info documents. */
static token_rule_t extrainfo_token_table[] = {
  T1_END( "router-signature",    K_ROUTER_SIGNATURE,    NO_ARGS, NEED_OBJ ),
  T1( "published",           K_PUBLISHED,       CONCAT_ARGS, NO_OBJ ),
  T01("identity-ed25519",    K_IDENTITY_ED25519,    NO_ARGS, NEED_OBJ ),
  T01("router-sig-ed25519",  K_ROUTER_SIG_ED25519,  GE(1),   NO_OBJ ),
  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),
  T01("read-history",        K_READ_HISTORY,        ARGS,    NO_OBJ ),
  T01("write-history",       K_WRITE_HISTORY,       ARGS,    NO_OBJ ),
  T01("dirreq-stats-end",    K_DIRREQ_END,          ARGS,    NO_OBJ ),
  T01("dirreq-v2-ips",       K_DIRREQ_V2_IPS,       ARGS,    NO_OBJ ),
  T01("dirreq-v3-ips",       K_DIRREQ_V3_IPS,       ARGS,    NO_OBJ ),
  T01("dirreq-v2-reqs",      K_DIRREQ_V2_REQS,      ARGS,    NO_OBJ ),
  T01("dirreq-v3-reqs",      K_DIRREQ_V3_REQS,      ARGS,    NO_OBJ ),
  T01("dirreq-v2-share",     K_DIRREQ_V2_SHARE,     ARGS,    NO_OBJ ),
  T01("dirreq-v3-share",     K_DIRREQ_V3_SHARE,     ARGS,    NO_OBJ ),
  T01("dirreq-v2-resp",      K_DIRREQ_V2_RESP,      ARGS,    NO_OBJ ),
  T01("dirreq-v3-resp",      K_DIRREQ_V3_RESP,      ARGS,    NO_OBJ ),
  T01("dirreq-v2-direct-dl", K_DIRREQ_V2_DIR,       ARGS,    NO_OBJ ),
  T01("dirreq-v3-direct-dl", K_DIRREQ_V3_DIR,       ARGS,    NO_OBJ ),
  T01("dirreq-v2-tunneled-dl", K_DIRREQ_V2_TUN,     ARGS,    NO_OBJ ),
  T01("dirreq-v3-tunneled-dl", K_DIRREQ_V3_TUN,     ARGS,    NO_OBJ ),
  T01("entry-stats-end",     K_ENTRY_END,           ARGS,    NO_OBJ ),
  T01("entry-ips",           K_ENTRY_IPS,           ARGS,    NO_OBJ ),
  T01("cell-stats-end",      K_CELL_END,            ARGS,    NO_OBJ ),
  T01("cell-processed-cells", K_CELL_PROCESSED,     ARGS,    NO_OBJ ),
  T01("cell-queued-cells",   K_CELL_QUEUED,         ARGS,    NO_OBJ ),
  T01("cell-time-in-queue",  K_CELL_TIME,           ARGS,    NO_OBJ ),
  T01("cell-circuits-per-decile", K_CELL_CIRCS,     ARGS,    NO_OBJ ),
  T01("exit-stats-end",      K_EXIT_END,            ARGS,    NO_OBJ ),
  T01("exit-kibibytes-written", K_EXIT_WRITTEN,     ARGS,    NO_OBJ ),
  T01("exit-kibibytes-read", K_EXIT_READ,           ARGS,    NO_OBJ ),
  T01("exit-streams-opened", K_EXIT_OPENED,         ARGS,    NO_OBJ ),

  T1_START( "extra-info",          K_EXTRA_INFO,          GE(2),   NO_OBJ ),

  END_OF_TABLE
};

/** List of tokens recognized in the body part of v3 networkstatus
 * documents. */
static token_rule_t rtrstatus_token_table[] = {
  T01("p",                   K_P,               CONCAT_ARGS, NO_OBJ ),
  T1( "r",                   K_R,                   GE(7),   NO_OBJ ),
  T0N("a",                   K_A,                   GE(1),   NO_OBJ ),
  T1( "s",                   K_S,                   ARGS,    NO_OBJ ),
  T01("v",                   K_V,               CONCAT_ARGS, NO_OBJ ),
  T01("w",                   K_W,                   ARGS,    NO_OBJ ),
  T0N("m",                   K_M,               CONCAT_ARGS, NO_OBJ ),
  T0N("id",                  K_ID,                  GE(2),   NO_OBJ ),
  T01("pr",                  K_PROTO,           CONCAT_ARGS, NO_OBJ ),
  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),
  END_OF_TABLE
};

/** List of tokens common to V3 authority certificates and V3 consensuses. */
#define CERTIFICATE_MEMBERS                                                  \
  T1("dir-key-certificate-version", K_DIR_KEY_CERTIFICATE_VERSION,           \
                                                     GE(1),       NO_OBJ ),  \
  T1("dir-identity-key", K_DIR_IDENTITY_KEY,         NO_ARGS,     NEED_KEY ),\
  T1("dir-key-published",K_DIR_KEY_PUBLISHED,        CONCAT_ARGS, NO_OBJ),   \
  T1("dir-key-expires",  K_DIR_KEY_EXPIRES,          CONCAT_ARGS, NO_OBJ),   \
  T1("dir-signing-key",  K_DIR_SIGNING_KEY,          NO_ARGS,     NEED_KEY ),\
  T1("dir-key-crosscert", K_DIR_KEY_CROSSCERT,       NO_ARGS,     NEED_OBJ ),\
  T1("dir-key-certification", K_DIR_KEY_CERTIFICATION,                       \
                                                     NO_ARGS,     NEED_OBJ), \
  T01("dir-address",     K_DIR_ADDRESS,              GE(1),       NO_OBJ),

/** List of tokens recognized in V3 authority certificates. */
static token_rule_t dir_key_certificate_table[] = {
  CERTIFICATE_MEMBERS
  T1("fingerprint",      K_FINGERPRINT,              CONCAT_ARGS, NO_OBJ ),
  END_OF_TABLE
};

/** List of tokens recognized in rendezvous service descriptors */
static token_rule_t desc_token_table[] = {
  T1_START("rendezvous-service-descriptor", R_RENDEZVOUS_SERVICE_DESCRIPTOR,
           EQ(1), NO_OBJ),
  T1("version", R_VERSION, EQ(1), NO_OBJ),
  T1("permanent-key", R_PERMANENT_KEY, NO_ARGS, NEED_KEY_1024),
  T1("secret-id-part", R_SECRET_ID_PART, EQ(1), NO_OBJ),
  T1("publication-time", R_PUBLICATION_TIME, CONCAT_ARGS, NO_OBJ),
  T1("protocol-versions", R_PROTOCOL_VERSIONS, EQ(1), NO_OBJ),
  T01("introduction-points", R_INTRODUCTION_POINTS, NO_ARGS, NEED_OBJ),
  T1_END("signature", R_SIGNATURE, NO_ARGS, NEED_OBJ),
  END_OF_TABLE
};

/** List of tokens recognized in the (encrypted) list of introduction points of
 * rendezvous service descriptors */
static token_rule_t ipo_token_table[] = {
  T1_START("introduction-point", R_IPO_IDENTIFIER, EQ(1), NO_OBJ),
  T1("ip-address", R_IPO_IP_ADDRESS, EQ(1), NO_OBJ),
  T1("onion-port", R_IPO_ONION_PORT, EQ(1), NO_OBJ),
  T1("onion-key", R_IPO_ONION_KEY, NO_ARGS, NEED_KEY_1024),
  T1("service-key", R_IPO_SERVICE_KEY, NO_ARGS, NEED_KEY_1024),
  END_OF_TABLE
};

/** List of tokens recognized in the (possibly encrypted) list of introduction
 * points of rendezvous service descriptors */
static token_rule_t client_keys_token_table[] = {
  T1_START("client-name", C_CLIENT_NAME, CONCAT_ARGS, NO_OBJ),
  T1("descriptor-cookie", C_DESCRIPTOR_COOKIE, EQ(1), NO_OBJ),
  T01("client-key", C_CLIENT_KEY, NO_ARGS, NEED_SKEY_1024),
  END_OF_TABLE
};

/** List of tokens recognized in V3 networkstatus votes. */
static token_rule_t networkstatus_token_table[] = {
  T1_START("network-status-version", K_NETWORK_STATUS_VERSION,
                                                   GE(1),       NO_OBJ ),
  T1("vote-status",            K_VOTE_STATUS,      GE(1),       NO_OBJ ),
  T1("published",              K_PUBLISHED,        CONCAT_ARGS, NO_OBJ ),
  T1("valid-after",            K_VALID_AFTER,      CONCAT_ARGS, NO_OBJ ),
  T1("fresh-until",            K_FRESH_UNTIL,      CONCAT_ARGS, NO_OBJ ),
  T1("valid-until",            K_VALID_UNTIL,      CONCAT_ARGS, NO_OBJ ),
  T1("voting-delay",           K_VOTING_DELAY,     GE(2),       NO_OBJ ),
  T1("known-flags",            K_KNOWN_FLAGS,      ARGS,        NO_OBJ ),
  T01("params",                K_PARAMS,           ARGS,        NO_OBJ ),
  T( "fingerprint",            K_FINGERPRINT,      CONCAT_ARGS, NO_OBJ ),
  T01("signing-ed25519",       K_SIGNING_CERT_ED,  NO_ARGS ,    NEED_OBJ ),
  T01("shared-rand-participate",K_SR_FLAG,         NO_ARGS,     NO_OBJ ),
  T0N("shared-rand-commit",    K_COMMIT,           GE(3),       NO_OBJ ),
  T01("shared-rand-previous-value", K_PREVIOUS_SRV,EQ(2),       NO_OBJ ),
  T01("shared-rand-current-value",  K_CURRENT_SRV, EQ(2),       NO_OBJ ),
  T0N("package",               K_PACKAGE,          CONCAT_ARGS, NO_OBJ ),
  T01("recommended-client-protocols", K_RECOMMENDED_CLIENT_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),
  T01("recommended-relay-protocols", K_RECOMMENDED_RELAY_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),
  T01("required-client-protocols",    K_REQUIRED_CLIENT_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),
  T01("required-relay-protocols",    K_REQUIRED_RELAY_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),

  CERTIFICATE_MEMBERS

  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),
  T1( "contact",             K_CONTACT,         CONCAT_ARGS, NO_OBJ ),
  T1( "dir-source",          K_DIR_SOURCE,      GE(6),       NO_OBJ ),
  T01("legacy-dir-key",      K_LEGACY_DIR_KEY,  GE(1),       NO_OBJ ),
  T1( "known-flags",         K_KNOWN_FLAGS,     CONCAT_ARGS, NO_OBJ ),
  T01("client-versions",     K_CLIENT_VERSIONS, CONCAT_ARGS, NO_OBJ ),
  T01("server-versions",     K_SERVER_VERSIONS, CONCAT_ARGS, NO_OBJ ),
  T1( "consensus-methods",   K_CONSENSUS_METHODS, GE(1),     NO_OBJ ),

  END_OF_TABLE
};

/** List of tokens recognized in V3 networkstatus consensuses. */
static token_rule_t networkstatus_consensus_token_table[] = {
  T1_START("network-status-version", K_NETWORK_STATUS_VERSION,
                                                   GE(1),       NO_OBJ ),
  T1("vote-status",            K_VOTE_STATUS,      GE(1),       NO_OBJ ),
  T1("valid-after",            K_VALID_AFTER,      CONCAT_ARGS, NO_OBJ ),
  T1("fresh-until",            K_FRESH_UNTIL,      CONCAT_ARGS, NO_OBJ ),
  T1("valid-until",            K_VALID_UNTIL,      CONCAT_ARGS, NO_OBJ ),
  T1("voting-delay",           K_VOTING_DELAY,     GE(2),       NO_OBJ ),

  T0N("opt",                 K_OPT,             CONCAT_ARGS, OBJ_OK ),

  T1N("dir-source",          K_DIR_SOURCE,          GE(6),   NO_OBJ ),
  T1N("contact",             K_CONTACT,         CONCAT_ARGS, NO_OBJ ),
  T1N("vote-digest",         K_VOTE_DIGEST,         GE(1),   NO_OBJ ),

  T1( "known-flags",         K_KNOWN_FLAGS,     CONCAT_ARGS, NO_OBJ ),

  T01("client-versions",     K_CLIENT_VERSIONS, CONCAT_ARGS, NO_OBJ ),
  T01("server-versions",     K_SERVER_VERSIONS, CONCAT_ARGS, NO_OBJ ),
  T01("consensus-method",    K_CONSENSUS_METHOD,    EQ(1),   NO_OBJ),
  T01("params",                K_PARAMS,           ARGS,        NO_OBJ ),

  T01("shared-rand-previous-value", K_PREVIOUS_SRV, EQ(2),   NO_OBJ ),
  T01("shared-rand-current-value",  K_CURRENT_SRV,  EQ(2),   NO_OBJ ),

  T01("recommended-client-protocols", K_RECOMMENDED_CLIENT_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),
  T01("recommended-relay-protocols", K_RECOMMENDED_RELAY_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),
  T01("required-client-protocols",    K_REQUIRED_CLIENT_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),
  T01("required-relay-protocols",    K_REQUIRED_RELAY_PROTOCOLS,
      CONCAT_ARGS, NO_OBJ ),

  END_OF_TABLE
};

/** List of tokens recognized in the footer of v1 directory footers. */
static token_rule_t networkstatus_vote_footer_token_table[] = {
  T01("directory-footer",    K_DIRECTORY_FOOTER,    NO_ARGS,   NO_OBJ ),
  T01("bandwidth-weights",   K_BW_WEIGHTS,          ARGS,      NO_OBJ ),
  T(  "directory-signature", K_DIRECTORY_SIGNATURE, GE(2),     NEED_OBJ ),
  END_OF_TABLE
};

/** List of tokens recognized in detached networkstatus signature documents. */
static token_rule_t networkstatus_detached_signature_token_table[] = {
  T1_START("consensus-digest", K_CONSENSUS_DIGEST, GE(1),       NO_OBJ ),
  T("additional-digest",       K_ADDITIONAL_DIGEST,GE(3),       NO_OBJ ),
  T1("valid-after",            K_VALID_AFTER,      CONCAT_ARGS, NO_OBJ ),
  T1("fresh-until",            K_FRESH_UNTIL,      CONCAT_ARGS, NO_OBJ ),
  T1("valid-until",            K_VALID_UNTIL,      CONCAT_ARGS, NO_OBJ ),
  T("additional-signature",  K_ADDITIONAL_SIGNATURE, GE(4),   NEED_OBJ ),
  T1N("directory-signature", K_DIRECTORY_SIGNATURE,  GE(2),   NEED_OBJ ),
  END_OF_TABLE
};

/** List of tokens recognized in microdescriptors */
static token_rule_t microdesc_token_table[] = {
  T1_START("onion-key",        K_ONION_KEY,        NO_ARGS,     NEED_KEY_1024),
  T01("ntor-onion-key",        K_ONION_KEY_NTOR,   GE(1),       NO_OBJ ),
  T0N("id",                    K_ID,               GE(2),       NO_OBJ ),
  T0N("a",                     K_A,                GE(1),       NO_OBJ ),
  T01("family",                K_FAMILY,           ARGS,        NO_OBJ ),
  T01("p",                     K_P,                CONCAT_ARGS, NO_OBJ ),
  T01("p6",                    K_P6,               CONCAT_ARGS, NO_OBJ ),
  A01("@last-listed",          A_LAST_LISTED,      CONCAT_ARGS, NO_OBJ ),
  END_OF_TABLE
};

#undef T

/* static function prototypes */
static int router_add_exit_policy(routerinfo_t *router,directory_token_t *tok);
static addr_policy_t *router_parse_addr_policy(directory_token_t *tok,
                                               unsigned fmt_flags);
static addr_policy_t *router_parse_addr_policy_private(directory_token_t *tok);

static int router_get_hash_impl_helper(const char *s, size_t s_len,
                            const char *start_str,
                            const char *end_str, char end_c,
                            const char **start_out, const char **end_out);
static int router_get_hash_impl(const char *s, size_t s_len, char *digest,
                                const char *start_str, const char *end_str,
                                char end_char,
                                digest_algorithm_t alg);
static int router_get_hashes_impl(const char *s, size_t s_len,
                                  common_digests_t *digests,
                                  const char *start_str, const char *end_str,
                                  char end_char);
static void token_clear(directory_token_t *tok);
static smartlist_t *find_all_by_keyword(smartlist_t *s, directory_keyword k);
static smartlist_t *find_all_exitpolicy(smartlist_t *s);
static directory_token_t *find_by_keyword_(smartlist_t *s,
                                           directory_keyword keyword,
                                           const char *keyword_str);
#define find_by_keyword(s, keyword) find_by_keyword_((s), (keyword), #keyword)
static directory_token_t *find_opt_by_keyword(smartlist_t *s,
                                              directory_keyword keyword);

#define TS_ANNOTATIONS_OK 1
#define TS_NOCHECK 2
#define TS_NO_NEW_ANNOTATIONS 4
static int tokenize_string(memarea_t *area,
                           const char *start, const char *end,
                           smartlist_t *out,
                           token_rule_t *table,
                           int flags);
static directory_token_t *get_next_token(memarea_t *area,
                                         const char **s,
                                         const char *eos,
                                         token_rule_t *table);
#define CST_CHECK_AUTHORITY   (1<<0)
#define CST_NO_CHECK_OBJTYPE  (1<<1)
static int check_signature_token(const char *digest,
                                 ssize_t digest_len,
                                 directory_token_t *tok,
                                 crypto_pk_t *pkey,
                                 int flags,
                                 const char *doctype);

#undef DEBUG_AREA_ALLOC

#ifdef DEBUG_AREA_ALLOC
#define DUMP_AREA(a,name) STMT_BEGIN                              \
  size_t alloc=0, used=0;                                         \
  memarea_get_stats((a),&alloc,&used);                            \
  log_debug(LD_MM, "Area for %s has %lu allocated; using %lu.",   \
            name, (unsigned long)alloc, (unsigned long)used);     \
  STMT_END
#else
#define DUMP_AREA(a,name) STMT_NIL
#endif

/* Dump mechanism for unparseable descriptors */

/** List of dumped descriptors for FIFO cleanup purposes */
STATIC smartlist_t *descs_dumped = NULL;
/** Total size of dumped descriptors for FIFO cleanup */
STATIC uint64_t len_descs_dumped = 0;
/** Directory to stash dumps in */
static int have_dump_desc_dir = 0;
static int problem_with_dump_desc_dir = 0;

#define DESC_DUMP_DATADIR_SUBDIR "unparseable-descs"
#define DESC_DUMP_BASE_FILENAME "unparseable-desc"

/** Find the dump directory and check if we'll be able to create it */
static void
dump_desc_init(void)
{
  char *dump_desc_dir;

  dump_desc_dir = get_datadir_fname(DESC_DUMP_DATADIR_SUBDIR);

  /*
   * We just check for it, don't create it at this point; we'll
   * create it when we need it if it isn't already there.
   */
  if (check_private_dir(dump_desc_dir, CPD_CHECK, get_options()->User) < 0) {
    /* Error, log and flag it as having a problem */
    log_notice(LD_DIR,
               "Doesn't look like we'll be able to create descriptor dump "
               "directory %s; dumps will be disabled.",
               dump_desc_dir);
    problem_with_dump_desc_dir = 1;
    tor_free(dump_desc_dir);
    return;
  }

  /* Check if it exists */
  switch (file_status(dump_desc_dir)) {
    case FN_DIR:
      /* We already have a directory */
      have_dump_desc_dir = 1;
      break;
    case FN_NOENT:
      /* Nothing, we'll need to create it later */
      have_dump_desc_dir = 0;
      break;
    case FN_ERROR:
      /* Log and flag having a problem */
      log_notice(LD_DIR,
                 "Couldn't check whether descriptor dump directory %s already"
                 " exists: %s",
                 dump_desc_dir, strerror(errno));
      problem_with_dump_desc_dir = 1;
      break;
    case FN_FILE:
    case FN_EMPTY:
    default:
      /* Something else was here! */
      log_notice(LD_DIR,
                 "Descriptor dump directory %s already exists and isn't a "
                 "directory",
                 dump_desc_dir);
      problem_with_dump_desc_dir = 1;
  }

  if (have_dump_desc_dir && !problem_with_dump_desc_dir) {
    dump_desc_populate_fifo_from_directory(dump_desc_dir);
  }

  tor_free(dump_desc_dir);
}

/** Create the dump directory if needed and possible */
static void
dump_desc_create_dir(void)
{
  char *dump_desc_dir;

  /* If the problem flag is set, skip it */
  if (problem_with_dump_desc_dir) return;

  /* Do we need it? */
  if (!have_dump_desc_dir) {
    dump_desc_dir = get_datadir_fname(DESC_DUMP_DATADIR_SUBDIR);

    if (check_private_dir(dump_desc_dir, CPD_CREATE,
                          get_options()->User) < 0) {
      log_notice(LD_DIR,
                 "Failed to create descriptor dump directory %s",
                 dump_desc_dir);
      problem_with_dump_desc_dir = 1;
    }

    /* Okay, we created it */
    have_dump_desc_dir = 1;

    tor_free(dump_desc_dir);
  }
}

/** Dump desc FIFO/cleanup; take ownership of the given filename, add it to
 * the FIFO, and clean up the oldest entries to the extent they exceed the
 * configured cap.  If any old entries with a matching hash existed, they
 * just got overwritten right before this was called and we should adjust
 * the total size counter without deleting them.
 */
static void
dump_desc_fifo_add_and_clean(char *filename, const uint8_t *digest_sha256,
                             size_t len)
{
  dumped_desc_t *ent = NULL, *tmp;
  uint64_t max_len;

  tor_assert(filename != NULL);
  tor_assert(digest_sha256 != NULL);

  if (descs_dumped == NULL) {
    /* We better have no length, then */
    tor_assert(len_descs_dumped == 0);
    /* Make a smartlist */
    descs_dumped = smartlist_new();
  }

  /* Make a new entry to put this one in */
  ent = tor_malloc_zero(sizeof(*ent));
  ent->filename = filename;
  ent->len = len;
  ent->when = time(NULL);
  memcpy(ent->digest_sha256, digest_sha256, DIGEST256_LEN);

  /* Do we need to do some cleanup? */
  max_len = get_options()->MaxUnparseableDescSizeToLog;
  /* Iterate over the list until we've freed enough space */
  while (len > max_len - len_descs_dumped &&
         smartlist_len(descs_dumped) > 0) {
    /* Get the oldest thing on the list */
    tmp = (dumped_desc_t *)(smartlist_get(descs_dumped, 0));

    /*
     * Check if it matches the filename we just added, so we don't delete
     * something we just emitted if we get repeated identical descriptors.
     */
    if (strcmp(tmp->filename, filename) != 0) {
      /* Delete it and adjust the length counter */
      tor_unlink(tmp->filename);
      tor_assert(len_descs_dumped >= tmp->len);
      len_descs_dumped -= tmp->len;
      log_info(LD_DIR,
               "Deleting old unparseable descriptor dump %s due to "
               "space limits",
               tmp->filename);
    } else {
      /*
       * Don't delete, but do adjust the counter since we will bump it
       * later
       */
      tor_assert(len_descs_dumped >= tmp->len);
      len_descs_dumped -= tmp->len;
      log_info(LD_DIR,
               "Replacing old descriptor dump %s with new identical one",
               tmp->filename);
    }

    /* Free it and remove it from the list */
    smartlist_del_keeporder(descs_dumped, 0);
    tor_free(tmp->filename);
    tor_free(tmp);
  }

  /* Append our entry to the end of the list and bump the counter */
  smartlist_add(descs_dumped, ent);
  len_descs_dumped += len;
}

/** Check if we already have a descriptor for this hash and move it to the
 * head of the queue if so.  Return 1 if one existed and 0 otherwise.
 */
static int
dump_desc_fifo_bump_hash(const uint8_t *digest_sha256)
{
  dumped_desc_t *match = NULL;

  tor_assert(digest_sha256);

  if (descs_dumped) {
    /* Find a match if one exists */
    SMARTLIST_FOREACH_BEGIN(descs_dumped, dumped_desc_t *, ent) {
      if (ent &&
          tor_memeq(ent->digest_sha256, digest_sha256, DIGEST256_LEN)) {
        /*
         * Save a pointer to the match and remove it from its current
         * position.
         */
        match = ent;
        SMARTLIST_DEL_CURRENT_KEEPORDER(descs_dumped, ent);
        break;
      }
    } SMARTLIST_FOREACH_END(ent);

    if (match) {
      /* Update the timestamp */
      match->when = time(NULL);
      /* Add it back at the end of the list */
      smartlist_add(descs_dumped, match);

      /* Indicate we found one */
      return 1;
    }
  }

  return 0;
}

/** Clean up on exit; just memory, leave the dumps behind
 */
STATIC void
dump_desc_fifo_cleanup(void)
{
  if (descs_dumped) {
    /* Free each descriptor */
    SMARTLIST_FOREACH_BEGIN(descs_dumped, dumped_desc_t *, ent) {
      tor_assert(ent);
      tor_free(ent->filename);
      tor_free(ent);
    } SMARTLIST_FOREACH_END(ent);
    /* Free the list */
    smartlist_free(descs_dumped);
    descs_dumped = NULL;
    len_descs_dumped = 0;
  }
}

/** Handle one file for dump_desc_populate_fifo_from_directory(); make sure
 * the filename is sensibly formed and matches the file content, and either
 * return a dumped_desc_t for it or remove the file and return NULL.
 */
MOCK_IMPL(STATIC dumped_desc_t *,
dump_desc_populate_one_file, (const char *dirname, const char *f))
{
  dumped_desc_t *ent = NULL;
  char *path = NULL, *desc = NULL;
  const char *digest_str;
  char digest[DIGEST256_LEN], content_digest[DIGEST256_LEN];
  /* Expected prefix before digest in filenames */
  const char *f_pfx = DESC_DUMP_BASE_FILENAME ".";
  /*
   * Stat while reading; this is important in case the file
   * contains a NUL character.
   */
  struct stat st;

  /* Sanity-check args */
  tor_assert(dirname != NULL);
  tor_assert(f != NULL);

  /* Form the full path */
  tor_asprintf(&path, "%s" PATH_SEPARATOR "%s", dirname, f);

  /* Check that f has the form DESC_DUMP_BASE_FILENAME.<digest256> */

  if (!strcmpstart(f, f_pfx)) {
    /* It matches the form, but is the digest parseable as such? */
    digest_str = f + strlen(f_pfx);
    if (base16_decode(digest, DIGEST256_LEN,
                      digest_str, strlen(digest_str)) != DIGEST256_LEN) {
      /* We failed to decode it */
      digest_str = NULL;
    }
  } else {
    /* No match */
    digest_str = NULL;
  }

  if (!digest_str) {
    /* We couldn't get a sensible digest */
    log_notice(LD_DIR,
               "Removing unrecognized filename %s from unparseable "
               "descriptors directory", f);
    tor_unlink(path);
    /* We're done */
    goto done;
  }

  /*
   * The filename has the form DESC_DUMP_BASE_FILENAME "." <digest256> and
   * we've decoded the digest.  Next, check that we can read it and the
   * content matches this digest.  We are relying on the fact that if the
   * file contains a '\0', read_file_to_str() will allocate space for and
   * read the entire file and return the correct size in st.
   */
  desc = read_file_to_str(path, RFTS_IGNORE_MISSING|RFTS_BIN, &st);
  if (!desc) {
    /* We couldn't read it */
    log_notice(LD_DIR,
               "Failed to read %s from unparseable descriptors directory; "
               "attempting to remove it.", f);
    tor_unlink(path);
    /* We're done */
    goto done;
  }

#if SIZE_MAX > UINT64_MAX
  if (BUG((uint64_t)st.st_size > (uint64_t)SIZE_MAX)) {
    /* LCOV_EXCL_START
     * Should be impossible since RFTS above should have failed to read the
     * huge file into RAM. */
    goto done;
    /* LCOV_EXCL_STOP */
  }
#endif
  if (BUG(st.st_size < 0)) {
    /* LCOV_EXCL_START
     * Should be impossible, since the OS isn't supposed to be b0rken. */
    goto done;
    /* LCOV_EXCL_STOP */
  }
  /* (Now we can be sure that st.st_size is safe to cast to a size_t.) */

  /*
   * We got one; now compute its digest and check that it matches the
   * filename.
   */
  if (crypto_digest256((char *)content_digest, desc, (size_t) st.st_size,
                       DIGEST_SHA256) != 0) {
    /* Weird, but okay */
    log_info(LD_DIR,
             "Unable to hash content of %s from unparseable descriptors "
             "directory", f);
    tor_unlink(path);
    /* We're done */
    goto done;
  }

  /* Compare the digests */
  if (tor_memneq(digest, content_digest, DIGEST256_LEN)) {
    /* No match */
    log_info(LD_DIR,
             "Hash of %s from unparseable descriptors directory didn't "
             "match its filename; removing it", f);
    tor_unlink(path);
    /* We're done */
    goto done;
  }

  /* Okay, it's a match, we should prepare ent */
  ent = tor_malloc_zero(sizeof(dumped_desc_t));
  ent->filename = path;
  memcpy(ent->digest_sha256, digest, DIGEST256_LEN);
  ent->len = (size_t) st.st_size;
  ent->when = st.st_mtime;
  /* Null out path so we don't free it out from under ent */
  path = NULL;

 done:
  /* Free allocations if we had them */
  tor_free(desc);
  tor_free(path);

  return ent;
}

/** Sort helper for dump_desc_populate_fifo_from_directory(); compares
 * the when field of dumped_desc_ts in a smartlist to put the FIFO in
 * the correct order after reconstructing it from the directory.
 */
static int
dump_desc_compare_fifo_entries(const void **a_v, const void **b_v)
{
  const dumped_desc_t **a = (const dumped_desc_t **)a_v;
  const dumped_desc_t **b = (const dumped_desc_t **)b_v;

  if ((a != NULL) && (*a != NULL)) {
    if ((b != NULL) && (*b != NULL)) {
      /* We have sensible dumped_desc_ts to compare */
      if ((*a)->when < (*b)->when) {
        return -1;
      } else if ((*a)->when == (*b)->when) {
        return 0;
      } else {
        return 1;
      }
    } else {
      /*
       * We shouldn't see this, but what the hell, NULLs precede everythin
       * else
       */
      return 1;
    }
  } else {
    return -1;
  }
}

/** Scan the contents of the directory, and update FIFO/counters; this will
 * consistency-check descriptor dump filenames against hashes of descriptor
 * dump file content, and remove any inconsistent/unreadable dumps, and then
 * reconstruct the dump FIFO as closely as possible for the last time the
 * tor process shut down.  If a previous dump was repeated more than once and
 * moved ahead in the FIFO, the mtime will not have been updated and the
 * reconstructed order will be wrong, but will always be a permutation of
 * the original.
 */
STATIC void
dump_desc_populate_fifo_from_directory(const char *dirname)
{
  smartlist_t *files = NULL;
  dumped_desc_t *ent = NULL;

  tor_assert(dirname != NULL);

  /* Get a list of files */
  files = tor_listdir(dirname);
  if (!files) {
    log_notice(LD_DIR,
               "Unable to get contents of unparseable descriptor dump "
               "directory %s",
               dirname);
    return;
  }

  /*
   * Iterate through the list and decide which files should go in the
   * FIFO and which should be purged.
   */

  SMARTLIST_FOREACH_BEGIN(files, char *, f) {
    /* Try to get a FIFO entry */
    ent = dump_desc_populate_one_file(dirname, f);
    if (ent) {
      /*
       * We got one; add it to the FIFO.  No need for duplicate checking
       * here since we just verified the name and digest match.
       */

      /* Make sure we have a list to add it to */
      if (!descs_dumped) {
        descs_dumped = smartlist_new();
        len_descs_dumped = 0;
      }

      /* Add it and adjust the counter */
      smartlist_add(descs_dumped, ent);
      len_descs_dumped += ent->len;
    }
    /*
     * If we didn't, we will have unlinked the file if necessary and
     * possible, and emitted a log message about it, so just go on to
     * the next.
     */
  } SMARTLIST_FOREACH_END(f);

  /* Did we get anything? */
  if (descs_dumped != NULL) {
    /* Sort the FIFO in order of increasing timestamp */
    smartlist_sort(descs_dumped, dump_desc_compare_fifo_entries);

    /* Log some stats */
    log_info(LD_DIR,
             "Reloaded unparseable descriptor dump FIFO with %d dump(s) "
             "totaling " U64_FORMAT " bytes",
             smartlist_len(descs_dumped), U64_PRINTF_ARG(len_descs_dumped));
  }

  /* Free the original list */
  SMARTLIST_FOREACH(files, char *, f, tor_free(f));
  smartlist_free(files);
}

/** For debugging purposes, dump unparseable descriptor *<b>desc</b> of
 * type *<b>type</b> to file $DATADIR/unparseable-desc. Do not write more
 * than one descriptor to disk per minute. If there is already such a
 * file in the data directory, overwrite it. */
STATIC void
dump_desc(const char *desc, const char *type)
{
  tor_assert(desc);
  tor_assert(type);
  size_t len;
  /* The SHA256 of the string */
  uint8_t digest_sha256[DIGEST256_LEN];
  char digest_sha256_hex[HEX_DIGEST256_LEN+1];
  /* Filename to log it to */
  char *debugfile, *debugfile_base;

  /* Get the hash for logging purposes anyway */
  len = strlen(desc);
  if (crypto_digest256((char *)digest_sha256, desc, len,
                       DIGEST_SHA256) != 0) {
    log_info(LD_DIR,
             "Unable to parse descriptor of type %s, and unable to even hash"
             " it!", type);
    goto err;
  }

  base16_encode(digest_sha256_hex, sizeof(digest_sha256_hex),
                (const char *)digest_sha256, sizeof(digest_sha256));

  /*
   * We mention type and hash in the main log; don't clutter up the files
   * with anything but the exact dump.
   */
  tor_asprintf(&debugfile_base,
               DESC_DUMP_BASE_FILENAME ".%s", digest_sha256_hex);
  debugfile = get_datadir_fname2(DESC_DUMP_DATADIR_SUBDIR, debugfile_base);

  /*
   * Check if the sandbox is active or will become active; see comment
   * below at the log message for why.
   */
  if (!(sandbox_is_active() || get_options()->Sandbox)) {
    if (len <= get_options()->MaxUnparseableDescSizeToLog) {
      if (!dump_desc_fifo_bump_hash(digest_sha256)) {
        /* Create the directory if needed */
        dump_desc_create_dir();
        /* Make sure we've got it */
        if (have_dump_desc_dir && !problem_with_dump_desc_dir) {
          /* Write it, and tell the main log about it */
          write_str_to_file(debugfile, desc, 1);
          log_info(LD_DIR,
                   "Unable to parse descriptor of type %s with hash %s and "
                   "length %lu. See file %s in data directory for details.",
                   type, digest_sha256_hex, (unsigned long)len,
                   debugfile_base);
          dump_desc_fifo_add_and_clean(debugfile, digest_sha256, len);
          /* Since we handed ownership over, don't free debugfile later */
          debugfile = NULL;
        } else {
          /* Problem with the subdirectory */
          log_info(LD_DIR,
                   "Unable to parse descriptor of type %s with hash %s and "
                   "length %lu. Descriptor not dumped because we had a "
                   "problem creating the " DESC_DUMP_DATADIR_SUBDIR
                   " subdirectory",
                   type, digest_sha256_hex, (unsigned long)len);
          /* We do have to free debugfile in this case */
        }
      } else {
        /* We already had one with this hash dumped */
        log_info(LD_DIR,
                 "Unable to parse descriptor of type %s with hash %s and "
                 "length %lu. Descriptor not dumped because one with that "
                 "hash has already been dumped.",
                 type, digest_sha256_hex, (unsigned long)len);
        /* We do have to free debugfile in this case */
      }
    } else {
      /* Just log that it happened without dumping */
      log_info(LD_DIR,
               "Unable to parse descriptor of type %s with hash %s and "
               "length %lu. Descriptor not dumped because it exceeds maximum"
               " log size all by itself.",
               type, digest_sha256_hex, (unsigned long)len);
      /* We do have to free debugfile in this case */
    }
  } else {
    /*
     * Not logging because the sandbox is active and seccomp2 apparently
     * doesn't have a sensible way to allow filenames according to a pattern
     * match.  (If we ever figure out how to say "allow writes to /regex/",
     * remove this checK).
     */
    log_info(LD_DIR,
             "Unable to parse descriptor of type %s with hash %s and "
             "length %lu. Descriptor not dumped because the sandbox is "
             "configured",
             type, digest_sha256_hex, (unsigned long)len);
  }

  tor_free(debugfile_base);
  tor_free(debugfile);

 err:
  return;
}

/** Set <b>digest</b> to the SHA-1 digest of the hash of the directory in
 * <b>s</b>.  Return 0 on success, -1 on failure.
 */
int
router_get_dir_hash(const char *s, char *digest)
{
  return router_get_hash_impl(s, strlen(s), digest,
                              "signed-directory","\ndirectory-signature",'\n',
                              DIGEST_SHA1);
}

/** Set <b>digest</b> to the SHA-1 digest of the hash of the first router in
 * <b>s</b>. Return 0 on success, -1 on failure.
 */
int
router_get_router_hash(const char *s, size_t s_len, char *digest)
{
  return router_get_hash_impl(s, s_len, digest,
                              "router ","\nrouter-signature", '\n',
                              DIGEST_SHA1);
}

/** Set <b>digests</b> to all the digests of the consensus document in
 * <b>s</b> */
int
router_get_networkstatus_v3_hashes(const char *s, common_digests_t *digests)
{
  return router_get_hashes_impl(s,strlen(s),digests,
                                "network-status-version",
                                "\ndirectory-signature",
                                ' ');
}

/** Set <b>digest</b> to the SHA-1 digest of the hash of the <b>s_len</b>-byte
 * extrainfo string at <b>s</b>.  Return 0 on success, -1 on failure. */
int
router_get_extrainfo_hash(const char *s, size_t s_len, char *digest)
{
  return router_get_hash_impl(s, s_len, digest, "extra-info",
                              "\nrouter-signature",'\n', DIGEST_SHA1);
}

/** Helper: used to generate signatures for routers, directories and
 * network-status objects.  Given a <b>digest_len</b>-byte digest in
 * <b>digest</b> and a secret <b>private_key</b>, generate an PKCS1-padded
 * signature, BASE64-encode it, surround it with -----BEGIN/END----- pairs,
 * and return the new signature on success or NULL on failure.
 */
char *
router_get_dirobj_signature(const char *digest,
                            size_t digest_len,
                            const crypto_pk_t *private_key)
{
  char *signature;
  size_t i, keysize;
  int siglen;
  char *buf = NULL;
  size_t buf_len;
  /* overestimate of BEGIN/END lines total len. */
#define BEGIN_END_OVERHEAD_LEN 64

  keysize = crypto_pk_keysize(private_key);
  signature = tor_malloc(keysize);
  siglen = crypto_pk_private_sign(private_key, signature, keysize,
                                  digest, digest_len);
  if (siglen < 0) {
    log_warn(LD_BUG,"Couldn't sign digest.");
    goto err;
  }

  /* The *2 here is a ridiculous overestimate of base-64 overhead. */
  buf_len = (siglen * 2) + BEGIN_END_OVERHEAD_LEN;
  buf = tor_malloc(buf_len);

  if (strlcpy(buf, "-----BEGIN SIGNATURE-----\n", buf_len) >= buf_len)
    goto truncated;

  i = strlen(buf);
  if (base64_encode(buf+i, buf_len-i, signature, siglen,
                    BASE64_ENCODE_MULTILINE) < 0) {
    log_warn(LD_BUG,"couldn't base64-encode signature");
    goto err;
  }

  if (strlcat(buf, "-----END SIGNATURE-----\n", buf_len) >= buf_len)
    goto truncated;

  tor_free(signature);
  return buf;

 truncated:
  log_warn(LD_BUG,"tried to exceed string length.");
 err:
  tor_free(signature);
  tor_free(buf);
  return NULL;
}

/** Helper: used to generate signatures for routers, directories and
 * network-status objects.  Given a digest in <b>digest</b> and a secret
 * <b>private_key</b>, generate a PKCS1-padded signature, BASE64-encode it,
 * surround it with -----BEGIN/END----- pairs, and write it to the
 * <b>buf_len</b>-byte buffer at <b>buf</b>.  Return 0 on success, -1 on
 * failure.
 */
int
router_append_dirobj_signature(char *buf, size_t buf_len, const char *digest,
                               size_t digest_len, crypto_pk_t *private_key)
{
  size_t sig_len, s_len;
  char *sig = router_get_dirobj_signature(digest, digest_len, private_key);
  if (!sig) {
    log_warn(LD_BUG, "No signature generated");
    return -1;
  }
  sig_len = strlen(sig);
  s_len = strlen(buf);
  if (sig_len + s_len + 1 > buf_len) {
    log_warn(LD_BUG, "Not enough room for signature");
    tor_free(sig);
    return -1;
  }
  memcpy(buf+s_len, sig, sig_len+1);
  tor_free(sig);
  return 0;
}

/** Return VS_RECOMMENDED if <b>myversion</b> is contained in
 * <b>versionlist</b>.  Else, return VS_EMPTY if versionlist has no
 * entries. Else, return VS_OLD if every member of
 * <b>versionlist</b> is newer than <b>myversion</b>.  Else, return
 * VS_NEW_IN_SERIES if there is at least one member of <b>versionlist</b> in
 * the same series (major.minor.micro) as <b>myversion</b>, but no such member
 * is newer than <b>myversion.</b>.  Else, return VS_NEW if every member of
 * <b>versionlist</b> is older than <b>myversion</b>.  Else, return
 * VS_UNRECOMMENDED.
 *
 * (versionlist is a comma-separated list of version strings,
 * optionally prefixed with "Tor".  Versions that can't be parsed are
 * ignored.)
 */
version_status_t
tor_version_is_obsolete(const char *myversion, const char *versionlist)
{
  tor_version_t mine, other;
  int found_newer = 0, found_older = 0, found_newer_in_series = 0,
    found_any_in_series = 0, r, same;
  version_status_t ret = VS_UNRECOMMENDED;
  smartlist_t *version_sl;

  log_debug(LD_CONFIG,"Checking whether version '%s' is in '%s'",
            myversion, versionlist);

  if (tor_version_parse(myversion, &mine)) {
    log_err(LD_BUG,"I couldn't parse my own version (%s)", myversion);
    tor_assert(0);
  }
  version_sl = smartlist_new();
  smartlist_split_string(version_sl, versionlist, ",", SPLIT_SKIP_SPACE, 0);

  if (!strlen(versionlist)) { /* no authorities cared or agreed */
    ret = VS_EMPTY;
    goto done;
  }

  SMARTLIST_FOREACH_BEGIN(version_sl, const char *, cp) {
    if (!strcmpstart(cp, "Tor "))
      cp += 4;

    if (tor_version_parse(cp, &other)) {
      /* Couldn't parse other; it can't be a match. */
    } else {
      same = tor_version_same_series(&mine, &other);
      if (same)
        found_any_in_series = 1;
      r = tor_version_compare(&mine, &other);
      if (r==0) {
        ret = VS_RECOMMENDED;
        goto done;
      } else if (r<0) {
        found_newer = 1;
        if (same)
          found_newer_in_series = 1;
      } else if (r>0) {
        found_older = 1;
      }
    }
  } SMARTLIST_FOREACH_END(cp);

  /* We didn't find the listed version. Is it new or old? */
  if (found_any_in_series && !found_newer_in_series && found_newer) {
    ret = VS_NEW_IN_SERIES;
  } else if (found_newer && !found_older) {
    ret = VS_OLD;
  } else if (found_older && !found_newer) {
    ret = VS_NEW;
  } else {
    ret = VS_UNRECOMMENDED;
  }

 done:
  SMARTLIST_FOREACH(version_sl, char *, version, tor_free(version));
  smartlist_free(version_sl);
  return ret;
}

/** Return true iff <b>key</b> is allowed to sign directories.
 */
static int
dir_signing_key_is_trusted(crypto_pk_t *key)
{
  char digest[DIGEST_LEN];
  if (!key) return 0;
  if (crypto_pk_get_digest(key, digest) < 0) {
    log_warn(LD_DIR, "Error computing dir-signing-key digest");
    return 0;
  }
  if (!router_digest_is_trusted_dir(digest)) {
    log_warn(LD_DIR, "Listed dir-signing-key is not trusted");
    return 0;
  }
  return 1;
}

/** Check whether the object body of the token in <b>tok</b> has a good
 * signature for <b>digest</b> using key <b>pkey</b>.  If
 * <b>CST_CHECK_AUTHORITY</b> is set, make sure that <b>pkey</b> is the key of
 * a directory authority.  If <b>CST_NO_CHECK_OBJTYPE</b> is set, do not check
 * the object type of the signature object. Use <b>doctype</b> as the type of
 * the document when generating log messages.  Return 0 on success, negative
 * on failure.
 */
static int
check_signature_token(const char *digest,
                      ssize_t digest_len,
                      directory_token_t *tok,
                      crypto_pk_t *pkey,
                      int flags,
                      const char *doctype)
{
  char *signed_digest;
  size_t keysize;
  const int check_authority = (flags & CST_CHECK_AUTHORITY);
  const int check_objtype = ! (flags & CST_NO_CHECK_OBJTYPE);

  tor_assert(pkey);
  tor_assert(tok);
  tor_assert(digest);
  tor_assert(doctype);

  if (check_authority && !dir_signing_key_is_trusted(pkey)) {
    log_warn(LD_DIR, "Key on %s did not come from an authority; rejecting",
             doctype);
    return -1;
  }

  if (check_objtype) {
    if (strcmp(tok->object_type, "SIGNATURE")) {
      log_warn(LD_DIR, "Bad object type on %s signature", doctype);
      return -1;
    }
  }

  keysize = crypto_pk_keysize(pkey);
  signed_digest = tor_malloc(keysize);
  if (crypto_pk_public_checksig(pkey, signed_digest, keysize,
                                tok->object_body, tok->object_size)
      < digest_len) {
    log_warn(LD_DIR, "Error reading %s: invalid signature.", doctype);
    tor_free(signed_digest);
    return -1;
  }
  //  log_debug(LD_DIR,"Signed %s hash starts %s", doctype,
  //            hex_str(signed_digest,4));
  if (tor_memneq(digest, signed_digest, digest_len)) {
    log_warn(LD_DIR, "Error reading %s: signature does not match.", doctype);
    tor_free(signed_digest);
    return -1;
  }
  tor_free(signed_digest);
  return 0;
}

/** Helper: move *<b>s_ptr</b> ahead to the next router, the next extra-info,
 * or to the first of the annotations proceeding the next router or
 * extra-info---whichever comes first.  Set <b>is_extrainfo_out</b> to true if
 * we found an extrainfo, or false if found a router. Do not scan beyond
 * <b>eos</b>.  Return -1 if we found nothing; 0 if we found something. */
static int
find_start_of_next_router_or_extrainfo(const char **s_ptr,
                                       const char *eos,
                                       int *is_extrainfo_out)
{
  const char *annotations = NULL;
  const char *s = *s_ptr;

  s = eat_whitespace_eos(s, eos);

  while (s < eos-32) {  /* 32 gives enough room for a the first keyword. */
    /* We're at the start of a line. */
    tor_assert(*s != '\n');

    if (*s == '@' && !annotations) {
      annotations = s;
    } else if (*s == 'r' && !strcmpstart(s, "router ")) {
      *s_ptr = annotations ? annotations : s;
      *is_extrainfo_out = 0;
      return 0;
    } else if (*s == 'e' && !strcmpstart(s, "extra-info ")) {
      *s_ptr = annotations ? annotations : s;
      *is_extrainfo_out = 1;
      return 0;
    }

    if (!(s = memchr(s+1, '\n', eos-(s+1))))
      break;
    s = eat_whitespace_eos(s, eos);
  }
  return -1;
}

/** Given a string *<b>s</b> containing a concatenated sequence of router
 * descriptors (or extra-info documents if <b>is_extrainfo</b> is set), parses
 * them and stores the result in <b>dest</b>.  All routers are marked running
 * and valid.  Advances *s to a point immediately following the last router
 * entry.  Ignore any trailing router entries that are not complete.
 *
 * If <b>saved_location</b> isn't SAVED_IN_CACHE, make a local copy of each
 * descriptor in the signed_descriptor_body field of each routerinfo_t.  If it
 * isn't SAVED_NOWHERE, remember the offset of each descriptor.
 *
 * Returns 0 on success and -1 on failure.  Adds a digest to
 * <b>invalid_digests_out</b> for every entry that was unparseable or
 * invalid. (This may cause duplicate entries.)
 */
int
router_parse_list_from_string(const char **s, const char *eos,
                              smartlist_t *dest,
                              saved_location_t saved_location,
                              int want_extrainfo,
                              int allow_annotations,
                              const char *prepend_annotations,
                              smartlist_t *invalid_digests_out)
{
  routerinfo_t *router;
  extrainfo_t *extrainfo;
  signed_descriptor_t *signed_desc = NULL;
  void *elt;
  const char *end, *start;
  int have_extrainfo;

  tor_assert(s);
  tor_assert(*s);
  tor_assert(dest);

  start = *s;
  if (!eos)
    eos = *s + strlen(*s);

  tor_assert(eos >= *s);

  while (1) {
    char raw_digest[DIGEST_LEN];
    int have_raw_digest = 0;
    int dl_again = 0;
    if (find_start_of_next_router_or_extrainfo(s, eos, &have_extrainfo) < 0)
      break;

    end = tor_memstr(*s, eos-*s, "\nrouter-signature");
    if (end)
      end = tor_memstr(end, eos-end, "\n-----END SIGNATURE-----\n");
    if (end)
      end += strlen("\n-----END SIGNATURE-----\n");

    if (!end)
      break;

    elt = NULL;

    if (have_extrainfo && want_extrainfo) {
      routerlist_t *rl = router_get_routerlist();
      have_raw_digest = router_get_extrainfo_hash(*s, end-*s, raw_digest) == 0;
      extrainfo = extrainfo_parse_entry_from_string(*s, end,
                                       saved_location != SAVED_IN_CACHE,
                                       rl->identity_map, &dl_again);
      if (extrainfo) {
        signed_desc = &extrainfo->cache_info;
        elt = extrainfo;
      }
    } else if (!have_extrainfo && !want_extrainfo) {
      have_raw_digest = router_get_router_hash(*s, end-*s, raw_digest) == 0;
      router = router_parse_entry_from_string(*s, end,
                                              saved_location != SAVED_IN_CACHE,
                                              allow_annotations,
                                              prepend_annotations, &dl_again);
      if (router) {
        log_debug(LD_DIR, "Read router '%s', purpose '%s'",
                  router_describe(router),
                  router_purpose_to_string(router->purpose));
        signed_desc = &router->cache_info;
        elt = router;
      }
    }
    if (! elt && ! dl_again && have_raw_digest && invalid_digests_out) {
      smartlist_add(invalid_digests_out, tor_memdup(raw_digest, DIGEST_LEN));
    }
    if (!elt) {
      *s = end;
      continue;
    }
    if (saved_location != SAVED_NOWHERE) {
      tor_assert(signed_desc);
      signed_desc->saved_location = saved_location;
      signed_desc->saved_offset = *s - start;
    }
    *s = end;
    smartlist_add(dest, elt);
  }

  return 0;
}

/* For debugging: define to count every descriptor digest we've seen so we
 * know if we need to try harder to avoid duplicate verifies. */
#undef COUNT_DISTINCT_DIGESTS

#ifdef COUNT_DISTINCT_DIGESTS
static digestmap_t *verified_digests = NULL;
#endif

/** Log the total count of the number of distinct router digests we've ever
 * verified.  When compared to the number of times we've verified routerdesc
 * signatures <i>in toto</i>, this will tell us if we're doing too much
 * multiple-verification. */
void
dump_distinct_digest_count(int severity)
{
#ifdef COUNT_DISTINCT_DIGESTS
  if (!verified_digests)
    verified_digests = digestmap_new();
  tor_log(severity, LD_GENERAL, "%d *distinct* router digests verified",
      digestmap_size(verified_digests));
#else
  (void)severity; /* suppress "unused parameter" warning */
#endif
}

/** Try to find an IPv6 OR port in <b>list</b> of directory_token_t's
 * with at least one argument (use GE(1) in setup). If found, store
 * address and port number to <b>addr_out</b> and
 * <b>port_out</b>. Return number of OR ports found. */
static int
find_single_ipv6_orport(const smartlist_t *list,
                        tor_addr_t *addr_out,
                        uint16_t *port_out)
{
  int ret = 0;
  tor_assert(list != NULL);
  tor_assert(addr_out != NULL);
  tor_assert(port_out != NULL);

  SMARTLIST_FOREACH_BEGIN(list, directory_token_t *, t) {
    tor_addr_t a;
    maskbits_t bits;
    uint16_t port_min, port_max;
    tor_assert(t->n_args >= 1);
    /* XXXX Prop186 the full spec allows much more than this. */
    if (tor_addr_parse_mask_ports(t->args[0], 0,
                                  &a, &bits, &port_min,
                                  &port_max) == AF_INET6 &&
        bits == 128 &&
        port_min == port_max) {
      /* Okay, this is one we can understand. Use it and ignore
         any potential more addresses in list. */
      tor_addr_copy(addr_out, &a);
      *port_out = port_min;
      ret = 1;
      break;
    }
  } SMARTLIST_FOREACH_END(t);

  return ret;
}

/** Helper function: reads a single router entry from *<b>s</b> ...
 * *<b>end</b>.  Mallocs a new router and returns it if all goes well, else
 * returns NULL.  If <b>cache_copy</b> is true, duplicate the contents of
 * s through end into the signed_descriptor_body of the resulting
 * routerinfo_t.
 *
 * If <b>end</b> is NULL, <b>s</b> must be properly NUL-terminated.
 *
 * If <b>allow_annotations</b>, it's okay to encounter annotations in <b>s</b>
 * before the router; if it's false, reject the router if it's annotated.  If
 * <b>prepend_annotations</b> is set, it should contain some annotations:
 * append them to the front of the router before parsing it, and keep them
 * around when caching the router.
 *
 * Only one of allow_annotations and prepend_annotations may be set.
 *
 * If <b>can_dl_again_out</b> is provided, set *<b>can_dl_again_out</b> to 1
 * if it's okay to try to download a descriptor with this same digest again,
 * and 0 if it isn't.  (It might not be okay to download it again if part of
 * the part covered by the digest is invalid.)
 */
routerinfo_t *
router_parse_entry_from_string(const char *s, const char *end,
                               int cache_copy, int allow_annotations,
                               const char *prepend_annotations,
                               int *can_dl_again_out)
{
  routerinfo_t *router = NULL;
  char digest[128];
  smartlist_t *tokens = NULL, *exit_policy_tokens = NULL;
  directory_token_t *tok;
  struct in_addr in;
  const char *start_of_annotations, *cp, *s_dup = s;
  size_t prepend_len = prepend_annotations ? strlen(prepend_annotations) : 0;
  int ok = 1;
  memarea_t *area = NULL;
  tor_cert_t *ntor_cc_cert = NULL;
  /* Do not set this to '1' until we have parsed everything that we intend to
   * parse that's covered by the hash. */
  int can_dl_again = 0;

  tor_assert(!allow_annotations || !prepend_annotations);

  if (!end) {
    end = s + strlen(s);
  }

  /* point 'end' to a point immediately after the final newline. */
  while (end > s+2 && *(end-1) == '\n' && *(end-2) == '\n')
    --end;

  area = memarea_new();
  tokens = smartlist_new();
  if (prepend_annotations) {
    if (tokenize_string(area,prepend_annotations,NULL,tokens,
                        routerdesc_token_table,TS_NOCHECK)) {
      log_warn(LD_DIR, "Error tokenizing router descriptor (annotations).");
      goto err;
    }
  }

  start_of_annotations = s;
  cp = tor_memstr(s, end-s, "\nrouter ");
  if (!cp) {
    if (end-s < 7 || strcmpstart(s, "router ")) {
      log_warn(LD_DIR, "No router keyword found.");
      goto err;
    }
  } else {
    s = cp+1;
  }

  if (start_of_annotations != s) { /* We have annotations */
    if (allow_annotations) {
      if (tokenize_string(area,start_of_annotations,s,tokens,
                          routerdesc_token_table,TS_NOCHECK)) {
        log_warn(LD_DIR, "Error tokenizing router descriptor (annotations).");
        goto err;
      }
    } else {
      log_warn(LD_DIR, "Found unexpected annotations on router descriptor not "
               "loaded from disk.  Dropping it.");
      goto err;
    }
  }

  if (router_get_router_hash(s, end - s, digest) < 0) {
    log_warn(LD_DIR, "Couldn't compute router hash.");
    goto err;
  }
  {
    int flags = 0;
    if (allow_annotations)
      flags |= TS_ANNOTATIONS_OK;
    if (prepend_annotations)
      flags |= TS_ANNOTATIONS_OK|TS_NO_NEW_ANNOTATIONS;

    if (tokenize_string(area,s,end,tokens,routerdesc_token_table, flags)) {
      log_warn(LD_DIR, "Error tokenizing router descriptor.");
      goto err;
    }
  }

  if (smartlist_len(tokens) < 2) {
    log_warn(LD_DIR, "Impossibly short router descriptor.");
    goto err;
  }

  tok = find_by_keyword(tokens, K_ROUTER);
  const int router_token_pos = smartlist_pos(tokens, tok);
  tor_assert(tok->n_args >= 5);

  router = tor_malloc_zero(sizeof(routerinfo_t));
  router->cert_expiration_time = TIME_MAX;
  router->cache_info.routerlist_index = -1;
  router->cache_info.annotations_len = s-start_of_annotations + prepend_len;
  router->cache_info.signed_descriptor_len = end-s;
  if (cache_copy) {
    size_t len = router->cache_info.signed_descriptor_len +
                 router->cache_info.annotations_len;
    char *signed_body =
      router->cache_info.signed_descriptor_body = tor_malloc(len+1);
    if (prepend_annotations) {
      memcpy(signed_body, prepend_annotations, prepend_len);
      signed_body += prepend_len;
    }
    /* This assertion will always succeed.
     * len == signed_desc_len + annotations_len
     *     == end-s + s-start_of_annotations + prepend_len
     *     == end-start_of_annotations + prepend_len
     * We already wrote prepend_len bytes into the buffer; now we're
     * writing end-start_of_annotations -NM. */
    tor_assert(signed_body+(end-start_of_annotations) ==
               router->cache_info.signed_descriptor_body+len);
    memcpy(signed_body, start_of_annotations, end-start_of_annotations);
    router->cache_info.signed_descriptor_body[len] = '\0';
    tor_assert(strlen(router->cache_info.signed_descriptor_body) == len);
  }
  memcpy(router->cache_info.signed_descriptor_digest, digest, DIGEST_LEN);

  router->nickname = tor_strdup(tok->args[0]);
  if (!is_legal_nickname(router->nickname)) {
    log_warn(LD_DIR,"Router nickname is invalid");
    goto err;
  }
  if (!tor_inet_aton(tok->args[1], &in)) {
    log_warn(LD_DIR,"Router address is not an IP address.");
    goto err;
  }
  router->addr = ntohl(in.s_addr);

  router->or_port =
    (uint16_t) tor_parse_long(tok->args[2],10,0,65535,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR,"Invalid OR port %s", escaped(tok->args[2]));
    goto err;
  }
  router->dir_port =
    (uint16_t) tor_parse_long(tok->args[4],10,0,65535,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR,"Invalid dir port %s", escaped(tok->args[4]));
    goto err;
  }

  tok = find_by_keyword(tokens, K_BANDWIDTH);
  tor_assert(tok->n_args >= 3);
  router->bandwidthrate = (int)
    tor_parse_long(tok->args[0],10,1,INT_MAX,&ok,NULL);

  if (!ok) {
    log_warn(LD_DIR, "bandwidthrate %s unreadable or 0. Failing.",
             escaped(tok->args[0]));
    goto err;
  }
  router->bandwidthburst =
    (int) tor_parse_long(tok->args[1],10,0,INT_MAX,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR, "Invalid bandwidthburst %s", escaped(tok->args[1]));
    goto err;
  }
  router->bandwidthcapacity = (int)
    tor_parse_long(tok->args[2],10,0,INT_MAX,&ok,NULL);
  if (!ok) {
    log_warn(LD_DIR, "Invalid bandwidthcapacity %s", escaped(tok->args[1]));
    goto err;
  }

  if ((tok = find_opt_by_keyword(tokens, A_PURPOSE))) {
    tor_assert(tok->n_args);
    router->purpose = router_purpose_from_string(tok->args[0]);
  } else {
    router->purpose = ROUTER_PURPOSE_GENERAL;
  }
  router->cache_info.send_unencrypted =
    (router->purpose == ROUTER_PURPOSE_GENERAL) ? 1 : 0;

  if ((tok = find_opt_by_keyword(tokens, K_UPTIME))) {
    tor_assert(tok->n_args >= 1);
    router->uptime = tor_parse_long(tok->args[0],10,0,LONG_MAX,&ok,NULL);
    if (!ok) {
      log_warn(LD_DIR, "Invalid uptime %s", escaped(tok->args[0]));
      goto err;
    }
  }

  if ((tok = find_opt_by_keyword(tokens, K_HIBERNATING))) {
    tor_assert(tok->n_args >= 1);
    router->is_hibernating
      = (tor_parse_long(tok->args[0],10,0,LONG_MAX,NULL,NULL) != 0);
  }

  tok = find_by_keyword(tokens, K_PUBLISHED);
  tor_assert(tok->n_args == 1);
  if (parse_iso_time(tok->args[0], &router->cache_info.published_on) < 0)
    goto err;

  tok = find_by_keyword(tokens, K_ONION_KEY);
  if (!crypto_pk_public_exponent_ok(tok->key)) {
    log_warn(LD_DIR,
             "Relay's onion key had invalid exponent.");
    goto err;
  }
  router->onion_pkey = tok->key;
  tok->key = NULL; /* Prevent free */

  if ((tok = find_opt_by_keyword(tokens, K_ONION_KEY_NTOR))) {
    curve25519_public_key_t k;
    tor_assert(tok->n_args >= 1);
    if (curve25519_public_from_base64(&k, tok->args[0]) < 0) {
      log_warn(LD_DIR, "Bogus ntor-onion-key in routerinfo");
      goto err;
    }
    router->onion_curve25519_pkey =
      tor_memdup(&k, sizeof(curve25519_public_key_t));
  }

  tok = find_by_keyword(tokens, K_SIGNING_KEY);
  router->identity_pkey = tok->key;
  tok->key = NULL; /* Prevent free */
  if (crypto_pk_get_digest(router->identity_pkey,
                           router->cache_info.identity_digest)) {
    log_warn(LD_DIR, "Couldn't calculate key digest"); goto err;
  }

  {
    directory_token_t *ed_sig_tok, *ed_cert_tok, *cc_tap_tok, *cc_ntor_tok,
      *master_key_tok;
    ed_sig_tok = find_opt_by_keyword(tokens, K_ROUTER_SIG_ED25519);
    ed_cert_tok = find_opt_by_keyword(tokens, K_IDENTITY_ED25519);
    master_key_tok = find_opt_by_keyword(tokens, K_MASTER_KEY_ED25519);
    cc_tap_tok = find_opt_by_keyword(tokens, K_ONION_KEY_CROSSCERT);
    cc_ntor_tok = find_opt_by_keyword(tokens, K_NTOR_ONION_KEY_CROSSCERT);
    int n_ed_toks = !!ed_sig_tok + !!ed_cert_tok +
      !!cc_tap_tok + !!cc_ntor_tok;
    if ((n_ed_toks != 0 && n_ed_toks != 4) ||
        (n_ed_toks == 4 && !router->onion_curve25519_pkey)) {
      log_warn(LD_DIR, "Router descriptor with only partial ed25519/"
               "cross-certification support");
      goto err;
    }
    if (master_key_tok && !ed_sig_tok) {
      log_warn(LD_DIR, "Router descriptor has ed25519 master key but no "
               "certificate");
      goto err;
    }
    if (ed_sig_tok) {
      tor_assert(ed_cert_tok && cc_tap_tok && cc_ntor_tok);
      const int ed_cert_token_pos = smartlist_pos(tokens, ed_cert_tok);
      if (ed_cert_token_pos == -1 || router_token_pos == -1 ||
          (ed_cert_token_pos != router_token_pos + 1 &&
           ed_cert_token_pos != router_token_pos - 1)) {
        log_warn(LD_DIR, "Ed25519 certificate in wrong position");
        goto err;
      }
      if (ed_sig_tok != smartlist_get(tokens, smartlist_len(tokens)-2)) {
        log_warn(LD_DIR, "Ed25519 signature in wrong position");
        goto err;
      }
      if (strcmp(ed_cert_tok->object_type, "ED25519 CERT")) {
        log_warn(LD_DIR, "Wrong object type on identity-ed25519 in decriptor");
        goto err;
      }
      if (strcmp(cc_ntor_tok->object_type, "ED25519 CERT")) {
        log_warn(LD_DIR, "Wrong object type on ntor-onion-key-crosscert "
                 "in decriptor");
        goto err;
      }
      if (strcmp(cc_tap_tok->object_type, "CROSSCERT")) {
        log_warn(LD_DIR, "Wrong object type on onion-key-crosscert "
                 "in decriptor");
        goto err;
      }
      if (strcmp(cc_ntor_tok->args[0], "0") &&
          strcmp(cc_ntor_tok->args[0], "1")) {
        log_warn(LD_DIR, "Bad sign bit on ntor-onion-key-crosscert");
        goto err;
      }
      int ntor_cc_sign_bit = !strcmp(cc_ntor_tok->args[0], "1");

      uint8_t d256[DIGEST256_LEN];
      const char *signed_start, *signed_end;
      tor_cert_t *cert = tor_cert_parse(
                       (const uint8_t*)ed_cert_tok->object_body,
                       ed_cert_tok->object_size);
      if (! cert) {
        log_warn(LD_DIR, "Couldn't parse ed25519 cert");
        goto err;
      }
      /* makes sure it gets freed. */
      router->cache_info.signing_key_cert = cert;

      if (cert->cert_type != CERT_TYPE_ID_SIGNING ||
          ! cert->signing_key_included) {
        log_warn(LD_DIR, "Invalid form for ed25519 cert");
        goto err;
      }

      if (master_key_tok) {
        /* This token is optional, but if it's present, it must match
         * the signature in the signing cert, or supplant it. */
        tor_assert(master_key_tok->n_args >= 1);
        ed25519_public_key_t pkey;
        if (ed25519_public_from_base64(&pkey, master_key_tok->args[0])<0) {
          log_warn(LD_DIR, "Can't parse ed25519 master key");
          goto err;
        }

        if (fast_memneq(&cert->signing_key.pubkey,
                        pkey.pubkey, ED25519_PUBKEY_LEN)) {
          log_warn(LD_DIR, "Ed25519 master key does not match "
                   "key in certificate");
          goto err;
        }
      }
      ntor_cc_cert = tor_cert_parse((const uint8_t*)cc_ntor_tok->object_body,
                                    cc_ntor_tok->object_size);
      if (!ntor_cc_cert) {
        log_warn(LD_DIR, "Couldn't parse ntor-onion-key-crosscert cert");
        goto err;
      }
      if (ntor_cc_cert->cert_type != CERT_TYPE_ONION_ID ||
          ! ed25519_pubkey_eq(&ntor_cc_cert->signed_key, &cert->signing_key)) {
        log_warn(LD_DIR, "Invalid contents for ntor-onion-key-crosscert cert");
        goto err;
      }

      ed25519_public_key_t ntor_cc_pk;
      if (ed25519_public_key_from_curve25519_public_key(&ntor_cc_pk,
                                            router->onion_curve25519_pkey,
                                            ntor_cc_sign_bit)<0) {
        log_warn(LD_DIR, "Error converting onion key to ed25519");
        goto err;
      }

      if (router_get_hash_impl_helper(s, end-s, "router ",
                                      "\nrouter-sig-ed25519",
                                      ' ', &signed_start, &signed_end) < 0) {
        log_warn(LD_DIR, "Can't find ed25519-signed portion of descriptor");
        goto err;
      }
      crypto_digest_t *d = crypto_digest256_new(DIGEST_SHA256);
      crypto_digest_add_bytes(d, ED_DESC_SIGNATURE_PREFIX,
        strlen(ED_DESC_SIGNATURE_PREFIX));
      crypto_digest_add_bytes(d, signed_start, signed_end-signed_start);
      crypto_digest_get_digest(d, (char*)d256, sizeof(d256));
      crypto_digest_free(d);

      ed25519_checkable_t check[3];
      int check_ok[3];
      if (tor_cert_get_checkable_sig(&check[0], cert, NULL) < 0) {
        log_err(LD_BUG, "Couldn't create 'checkable' for cert.");
        goto err;
      }
      if (tor_cert_get_checkable_sig(&check[1],
                                     ntor_cc_cert, &ntor_cc_pk) < 0) {
        log_err(LD_BUG, "Couldn't create 'checkable' for ntor_cc_cert.");
        goto err;
      }

      if (ed25519_signature_from_base64(&check[2].signature,
                                        ed_sig_tok->args[0])<0) {
        log_warn(LD_DIR, "Couldn't decode ed25519 signature");
        goto err;
      }
      check[2].pubkey = &cert->signed_key;
      check[2].msg = d256;
      check[2].len = DIGEST256_LEN;

      if (ed25519_checksig_batch(check_ok, check, 3) < 0) {
        log_warn(LD_DIR, "Incorrect ed25519 signature(s)");
        goto err;
      }

      if (check_tap_onion_key_crosscert(
                      (const uint8_t*)cc_tap_tok->object_body,
                      (int)cc_tap_tok->object_size,
                      router->onion_pkey,
                      &cert->signing_key,
                      (const uint8_t*)router->cache_info.identity_digest)<0) {
        log_warn(LD_DIR, "Incorrect TAP cross-verification");
        goto err;
      }

      /* We check this before adding it to the routerlist. */
      if (cert->valid_until < ntor_cc_cert->valid_until)
        router->cert_expiration_time = cert->valid_until;
      else
        router->cert_expiration_time = ntor_cc_cert->valid_until;
    }
  }

  if ((tok = find_opt_by_keyword(tokens, K_FINGERPRINT))) {
    /* If there's a fingerprint line, it must match the identity digest. */
    char d[DIGEST_LEN];
    tor_assert(tok->n_args == 1);
    tor_strstrip(tok->args[0], " ");
    if (base16_decode(d, DIGEST_LEN,
                      tok->args[0], strlen(tok->args[0])) != DIGEST_LEN) {
      log_warn(LD_DIR, "Couldn't decode router fingerprint %s",
               escaped(tok->args[0]));
      goto err;
    }
    if (tor_memneq(d,router->cache_info.identity_digest, DIGEST_LEN)) {
      log_warn(LD_DIR, "Fingerprint '%s' does not match identity digest.",
               tok->args[0]);
      goto err;
    }
  }

  if ((tok = find_opt_by_keyword(tokens, K_PLATFORM))) {
    router->platform = tor_strdup(tok->args[0]);
  }

  if ((tok = find_opt_by_keyword(tokens, K_PROTO))) {
    router->protocol_list = tor_strdup(tok->args[0]);
  }

  if ((tok = find_opt_by_keyword(tokens, K_CONTACT))) {
    router->contact_info = tor_strdup(tok->args[0]);
  }

  if (find_opt_by_keyword(tokens, K_REJECT6) ||
      find_opt_by_keyword(tokens, K_ACCEPT6)) {
    log_warn(LD_DIR, "Rejecting router with reject6/accept6 line: they crash "
             "older Tors.");
    goto err;
  }
  {
    smartlist_t *or_addresses = find_all_by_keyword(tokens, K_OR_ADDRESS);
    if (or_addresses) {
      find_single_ipv6_orport(or_addresses, &router->ipv6_addr,
                              &router->ipv6_orport);
      smartlist_free(or_addresses);
    }
  }
  exit_policy_tokens = find_all_exitpolicy(tokens);
  if (!smartlist_len(exit_policy_tokens)) {
    log_warn(LD_DIR, "No exit policy tokens in descriptor.");
    goto err;
  }
  SMARTLIST_FOREACH(exit_policy_tokens, directory_token_t *, t,
                    if (router_add_exit_policy(router,t)<0) {
                      log_warn(LD_DIR,"Error in exit policy");
                      goto err;
                    });
  policy_expand_private(&router->exit_policy);

  if ((tok = find_opt_by_keyword(tokens, K_IPV6_POLICY)) && tok->n_args) {
    router->ipv6_exit_policy = parse_short_policy(tok->args[0]);
    if (! router->ipv6_exit_policy) {
      log_warn(LD_DIR , "Error in ipv6-policy %s", escaped(tok->args[0]));
      goto err;
    }
  }

  if (policy_is_reject_star(router->exit_policy, AF_INET, 1) &&
      (!router->ipv6_exit_policy ||
       short_policy_is_reject_star(router->ipv6_exit_policy)))
    router->policy_is_reject_star = 1;

  if ((tok = find_opt_by_keyword(tokens, K_FAMILY)) && tok->n_args) {
    int i;
    router->declared_family = smartlist_new();
    for (i=0;i<tok->n_args;++i) {
      if (!is_legal_nickname_or_hexdigest(tok->args[i])) {
        log_warn(LD_DIR, "Illegal nickname %s in family line",
                 escaped(tok->args[i]));
        goto err;
      }
      smartlist_add(router->declared_family, tor_strdup(tok->args[i]));
    }
  }

  if (find_opt_by_keyword(tokens, K_CACHES_EXTRA_INFO))
    router->caches_extra_info = 1;

  if (find_opt_by_keyword(tokens, K_ALLOW_SINGLE_HOP_EXITS))
    router->allow_single_hop_exits = 1;

  if ((tok = find_opt_by_keyword(tokens, K_EXTRA_INFO_DIGEST))) {
    tor_assert(tok->n_args >= 1);
    if (strlen(tok->args[0]) == HEX_DIGEST_LEN) {
      if (base16_decode(router->cache_info.extra_info_digest, DIGEST_LEN,
                        tok->args[0], HEX_DIGEST_LEN) != DIGEST_LEN) {
          log_warn(LD_DIR,"Invalid extra info digest");
      }
    } else {
      log_warn(LD_DIR, "Invalid extra info digest %s", escaped(tok->args[0]));
    }

    if (tok->n_args >= 2) {
      if (digest256_from_base64(router->cache_info.extra_info_digest256,
                                tok->args[1]) < 0) {
        log_warn(LD_DIR, "Invalid extra info digest256 %s",
                 escaped(tok->args[1]));
      }
    }
  }

  if (find_opt_by_keyword(tokens, K_HIDDEN_SERVICE_DIR)) {
    router->wants_to_be_hs_dir = 1;
  }

  /* This router accepts tunnelled directory requests via begindir if it has
   * an open dirport or it included "tunnelled-dir-server". */
  if (find_opt_by_keyword(tokens, K_DIR_TUNNELLED) || router->dir_port > 0) {
    router->supports_tunnelled_dir_requests = 1;
  }

  tok = find_by_keyword(tokens, K_ROUTER_SIGNATURE);
  note_crypto_pk_op(VERIFY_RTR);
#ifdef COUNT_DISTINCT_DIGESTS
  if (!verified_digests)
    verified_digests = digestmap_new();
  digestmap_set(verified_digests, signed_digest, (void*)(uintptr_t)1);
#endif

  if (!router->or_port) {
    log_warn(LD_DIR,"or_port unreadable or 0. Failing.");
    goto err;
  }

  /* We've checked everything that's covered by the hash. */
  can_dl_again = 1;
  if (check_signature_token(digest, DIGEST_LEN, tok, router->identity_pkey, 0,
                            "router descriptor") < 0)
    goto err;

  if (!router->platform) {
    router->platform = tor_strdup("<unknown>");
  }
  goto done;

 err:
  dump_desc(s_dup, "router descriptor");
  routerinfo_free(router);
  router = NULL;
 done:
  tor_cert_free(ntor_cc_cert);
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  smartlist_free(exit_policy_tokens);
  if (area) {
    DUMP_AREA(area, "routerinfo");
    memarea_drop_all(area);
  }
  if (can_dl_again_out)
    *can_dl_again_out = can_dl_again;
  return router;
}

/** Parse a single extrainfo entry from the string <b>s</b>, ending at
 * <b>end</b>.  (If <b>end</b> is NULL, parse up to the end of <b>s</b>.)  If
 * <b>cache_copy</b> is true, make a copy of the extra-info document in the
 * cache_info fields of the result.  If <b>routermap</b> is provided, use it
 * as a map from router identity to routerinfo_t when looking up signing keys.
 *
 * If <b>can_dl_again_out</b> is provided, set *<b>can_dl_again_out</b> to 1
 * if it's okay to try to download an extrainfo with this same digest again,
 * and 0 if it isn't.  (It might not be okay to download it again if part of
 * the part covered by the digest is invalid.)
 */
extrainfo_t *
extrainfo_parse_entry_from_string(const char *s, const char *end,
                            int cache_copy, struct digest_ri_map_t *routermap,
                            int *can_dl_again_out)
{
  extrainfo_t *extrainfo = NULL;
  char digest[128];
  smartlist_t *tokens = NULL;
  directory_token_t *tok;
  crypto_pk_t *key = NULL;
  routerinfo_t *router = NULL;
  memarea_t *area = NULL;
  const char *s_dup = s;
  /* Do not set this to '1' until we have parsed everything that we intend to
   * parse that's covered by the hash. */
  int can_dl_again = 0;

  if (!end) {
    end = s + strlen(s);
  }

  /* point 'end' to a point immediately after the final newline. */
  while (end > s+2 && *(end-1) == '\n' && *(end-2) == '\n')
    --end;

  if (router_get_extrainfo_hash(s, end-s, digest) < 0) {
    log_warn(LD_DIR, "Couldn't compute router hash.");
    goto err;
  }
  tokens = smartlist_new();
  area = memarea_new();
  if (tokenize_string(area,s,end,tokens,extrainfo_token_table,0)) {
    log_warn(LD_DIR, "Error tokenizing extra-info document.");
    goto err;
  }

  if (smartlist_len(tokens) < 2) {
    log_warn(LD_DIR, "Impossibly short extra-info document.");
    goto err;
  }

  /* XXXX Accept this in position 1 too, and ed identity in position 0. */
  tok = smartlist_get(tokens,0);
  if (tok->tp != K_EXTRA_INFO) {
    log_warn(LD_DIR,"Entry does not start with \"extra-info\"");
    goto err;
  }

  extrainfo = tor_malloc_zero(sizeof(extrainfo_t));
  extrainfo->cache_info.is_extrainfo = 1;
  if (cache_copy)
    extrainfo->cache_info.signed_descriptor_body = tor_memdup_nulterm(s,end-s);
  extrainfo->cache_info.signed_descriptor_len = end-s;
  memcpy(extrainfo->cache_info.signed_descriptor_digest, digest, DIGEST_LEN);
  crypto_digest256((char*)extrainfo->digest256, s, end-s, DIGEST_SHA256);

  tor_assert(tok->n_args >= 2);
  if (!is_legal_nickname(tok->args[0])) {
    log_warn(LD_DIR,"Bad nickname %s on \"extra-info\"",escaped(tok->args[0]));
    goto err;
  }
  strlcpy(extrainfo->nickname, tok->args[0], sizeof(extrainfo->nickname));
  if (strlen(tok->args[1]) != HEX_DIGEST_LEN ||
      base16_decode(extrainfo->cache_info.identity_digest, DIGEST_LEN,
                    tok->args[1], HEX_DIGEST_LEN) != DIGEST_LEN) {
    log_warn(LD_DIR,"Invalid fingerprint %s on \"extra-info\"",
             escaped(tok->args[1]));
    goto err;
  }

  tok = find_by_keyword(tokens, K_PUBLISHED);
  if (parse_iso_time(tok->args[0], &extrainfo->cache_info.published_on)) {
    log_warn(LD_DIR,"Invalid published time %s on \"extra-info\"",
             escaped(tok->args[0]));
    goto err;
  }

  {
    directory_token_t *ed_sig_tok, *ed_cert_tok;
    ed_sig_tok = find_opt_by_keyword(tokens, K_ROUTER_SIG_ED25519);
    ed_cert_tok = find_opt_by_keyword(tokens, K_IDENTITY_ED25519);
    int n_ed_toks = !!ed_sig_tok + !!ed_cert_tok;
    if (n_ed_toks != 0 && n_ed_toks != 2) {
      log_warn(LD_DIR, "Router descriptor with only partial ed25519/"
               "cross-certification support");
      goto err;
    }
    if (ed_sig_tok) {
      tor_assert(ed_cert_tok);
      const int ed_cert_token_pos = smartlist_pos(tokens, ed_cert_tok);
      if (ed_cert_token_pos != 1) {
        /* Accept this in position 0 XXXX */
        log_warn(LD_DIR, "Ed25519 certificate in wrong position");
        goto err;
      }
      if (ed_sig_tok != smartlist_get(tokens, smartlist_len(tokens)-2)) {
        log_warn(LD_DIR, "Ed25519 signature in wrong position");
        goto err;
      }
      if (strcmp(ed_cert_tok->object_type, "ED25519 CERT")) {
        log_warn(LD_DIR, "Wrong object type on identity-ed25519 in decriptor");
        goto err;
      }

      uint8_t d256[DIGEST256_LEN];
      const char *signed_start, *signed_end;
      tor_cert_t *cert = tor_cert_parse(
                       (const uint8_t*)ed_cert_tok->object_body,
                       ed_cert_tok->object_size);
      if (! cert) {
        log_warn(LD_DIR, "Couldn't parse ed25519 cert");
        goto err;
      }
      /* makes sure it gets freed. */
      extrainfo->cache_info.signing_key_cert = cert;

      if (cert->cert_type != CERT_TYPE_ID_SIGNING ||
          ! cert->signing_key_included) {
        log_warn(LD_DIR, "Invalid form for ed25519 cert");
        goto err;
      }

      if (router_get_hash_impl_helper(s, end-s, "extra-info ",
                                      "\nrouter-sig-ed25519",
                                      ' ', &signed_start, &signed_end) < 0) {
        log_warn(LD_DIR, "Can't find ed25519-signed portion of extrainfo");
        goto err;
      }
      crypto_digest_t *d = crypto_digest256_new(DIGEST_SHA256);
      crypto_digest_add_bytes(d, ED_DESC_SIGNATURE_PREFIX,
        strlen(ED_DESC_SIGNATURE_PREFIX));
      crypto_digest_add_bytes(d, signed_start, signed_end-signed_start);
      crypto_digest_get_digest(d, (char*)d256, sizeof(d256));
      crypto_digest_free(d);

      ed25519_checkable_t check[2];
      int check_ok[2];
      if (tor_cert_get_checkable_sig(&check[0], cert, NULL) < 0) {
        log_err(LD_BUG, "Couldn't create 'checkable' for cert.");
        goto err;
      }

      if (ed25519_signature_from_base64(&check[1].signature,
                                        ed_sig_tok->args[0])<0) {
        log_warn(LD_DIR, "Couldn't decode ed25519 signature");
        goto err;
      }
      check[1].pubkey = &cert->signed_key;
      check[1].msg = d256;
      check[1].len = DIGEST256_LEN;

      if (ed25519_checksig_batch(check_ok, check, 2) < 0) {
        log_warn(LD_DIR, "Incorrect ed25519 signature(s)");
        goto err;
      }
      /* We don't check the certificate expiration time: checking that it
       * matches the cert in the router descriptor is adequate. */
    }
  }

  /* We've checked everything that's covered by the hash. */
  can_dl_again = 1;

  if (routermap &&
      (router = digestmap_get((digestmap_t*)routermap,
                              extrainfo->cache_info.identity_digest))) {
    key = router->identity_pkey;
  }

  tok = find_by_keyword(tokens, K_ROUTER_SIGNATURE);
  if (strcmp(tok->object_type, "SIGNATURE") ||
      tok->object_size < 128 || tok->object_size > 512) {
    log_warn(LD_DIR, "Bad object type or length on extra-info signature");
    goto err;
  }

  if (key) {
    note_crypto_pk_op(VERIFY_RTR);
    if (check_signature_token(digest, DIGEST_LEN, tok, key, 0,
                              "extra-info") < 0)
      goto err;

    if (router)
      extrainfo->cache_info.send_unencrypted =
        router->cache_info.send_unencrypted;
  } else {
    extrainfo->pending_sig = tor_memdup(tok->object_body,
                                        tok->object_size);
    extrainfo->pending_sig_len = tok->object_size;
  }

  goto done;
 err:
  dump_desc(s_dup, "extra-info descriptor");
  extrainfo_free(extrainfo);
  extrainfo = NULL;
 done:
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  if (area) {
    DUMP_AREA(area, "extrainfo");
    memarea_drop_all(area);
  }
  if (can_dl_again_out)
    *can_dl_again_out = can_dl_again;
  return extrainfo;
}

/** Parse a key certificate from <b>s</b>; point <b>end-of-string</b> to
 * the first character after the certificate. */
authority_cert_t *
authority_cert_parse_from_string(const char *s, const char **end_of_string)
{
  /** Reject any certificate at least this big; it is probably an overflow, an
   * attack, a bug, or some other nonsense. */
#define MAX_CERT_SIZE (128*1024)

  authority_cert_t *cert = NULL, *old_cert;
  smartlist_t *tokens = NULL;
  char digest[DIGEST_LEN];
  directory_token_t *tok;
  char fp_declared[DIGEST_LEN];
  char *eos;
  size_t len;
  int found;
  memarea_t *area = NULL;
  const char *s_dup = s;

  s = eat_whitespace(s);
  eos = strstr(s, "\ndir-key-certification");
  if (! eos) {
    log_warn(LD_DIR, "No signature found on key certificate");
    return NULL;
  }
  eos = strstr(eos, "\n-----END SIGNATURE-----\n");
  if (! eos) {
    log_warn(LD_DIR, "No end-of-signature found on key certificate");
    return NULL;
  }
  eos = strchr(eos+2, '\n');
  tor_assert(eos);
  ++eos;
  len = eos - s;

  if (len > MAX_CERT_SIZE) {
    log_warn(LD_DIR, "Certificate is far too big (at %lu bytes long); "
             "rejecting", (unsigned long)len);
    return NULL;
  }

  tokens = smartlist_new();
  area = memarea_new();
  if (tokenize_string(area,s, eos, tokens, dir_key_certificate_table, 0) < 0) {
    log_warn(LD_DIR, "Error tokenizing key certificate");
    goto err;
  }
  if (router_get_hash_impl(s, strlen(s), digest, "dir-key-certificate-version",
                           "\ndir-key-certification", '\n', DIGEST_SHA1) < 0)
    goto err;
  tok = smartlist_get(tokens, 0);
  if (tok->tp != K_DIR_KEY_CERTIFICATE_VERSION || strcmp(tok->args[0], "3")) {
    log_warn(LD_DIR,
             "Key certificate does not begin with a recognized version (3).");
    goto err;
  }

  cert = tor_malloc_zero(sizeof(authority_cert_t));
  memcpy(cert->cache_info.signed_descriptor_digest, digest, DIGEST_LEN);

  tok = find_by_keyword(tokens, K_DIR_SIGNING_KEY);
  tor_assert(tok->key);
  cert->signing_key = tok->key;
  tok->key = NULL;
  if (crypto_pk_get_digest(cert->signing_key, cert->signing_key_digest))
    goto err;

  tok = find_by_keyword(tokens, K_DIR_IDENTITY_KEY);
  tor_assert(tok->key);
  cert->identity_key = tok->key;
  tok->key = NULL;

  tok = find_by_keyword(tokens, K_FINGERPRINT);
  tor_assert(tok->n_args);
  if (base16_decode(fp_declared, DIGEST_LEN, tok->args[0],
                    strlen(tok->args[0])) != DIGEST_LEN) {
    log_warn(LD_DIR, "Couldn't decode key certificate fingerprint %s",
             escaped(tok->args[0]));
    goto err;
  }

  if (crypto_pk_get_digest(cert->identity_key,
                           cert->cache_info.identity_digest))
    goto err;

  if (tor_memneq(cert->cache_info.identity_digest, fp_declared, DIGEST_LEN)) {
    log_warn(LD_DIR, "Digest of certificate key didn't match declared "
             "fingerprint");
    goto err;
  }

  tok = find_opt_by_keyword(tokens, K_DIR_ADDRESS);
  if (tok) {
    struct in_addr in;
    char *address = NULL;
    tor_assert(tok->n_args);
    /* XXX++ use some tor_addr parse function below instead. -RD */
    if (tor_addr_port_split(LOG_WARN, tok->args[0], &address,
                            &cert->dir_port) < 0 ||
        tor_inet_aton(address, &in) == 0) {
      log_warn(LD_DIR, "Couldn't parse dir-address in certificate");
      tor_free(address);
      goto err;
    }
    cert->addr = ntohl(in.s_addr);
    tor_free(address);
  }

  tok = find_by_keyword(tokens, K_DIR_KEY_PUBLISHED);
  if (parse_iso_time(tok->args[0], &cert->cache_info.published_on) < 0) {
     goto err;
  }
  tok = find_by_keyword(tokens, K_DIR_KEY_EXPIRES);
  if (parse_iso_time(tok->args[0], &cert->expires) < 0) {
     goto err;
  }

  tok = smartlist_get(tokens, smartlist_len(tokens)-1);
  if (tok->tp != K_DIR_KEY_CERTIFICATION) {
    log_warn(LD_DIR, "Certificate didn't end with dir-key-certification.");
    goto err;
  }

  /* If we already have this cert, don't bother checking the signature. */
  old_cert = authority_cert_get_by_digests(
                                     cert->cache_info.identity_digest,
                                     cert->signing_key_digest);
  found = 0;
  if (old_cert) {
    /* XXXX We could just compare signed_descriptor_digest, but that wouldn't
     * buy us much. */
    if (old_cert->cache_info.signed_descriptor_len == len &&
        old_cert->cache_info.signed_descriptor_body &&
        tor_memeq(s, old_cert->cache_info.signed_descriptor_body, len)) {
      log_debug(LD_DIR, "We already checked the signature on this "
                "certificate; no need to do so again.");
      found = 1;
    }
  }
  if (!found) {
    if (check_signature_token(digest, DIGEST_LEN, tok, cert->identity_key, 0,
                              "key certificate")) {
      goto err;
    }

    tok = find_by_keyword(tokens, K_DIR_KEY_CROSSCERT);
    if (check_signature_token(cert->cache_info.identity_digest,
                              DIGEST_LEN,
                              tok,
                              cert->signing_key,
                              CST_NO_CHECK_OBJTYPE,
                              "key cross-certification")) {
      goto err;
    }
  }

  cert->cache_info.signed_descriptor_len = len;
  cert->cache_info.signed_descriptor_body = tor_malloc(len+1);
  memcpy(cert->cache_info.signed_descriptor_body, s, len);
  cert->cache_info.signed_descriptor_body[len] = 0;
  cert->cache_info.saved_location = SAVED_NOWHERE;

  if (end_of_string) {
    *end_of_string = eat_whitespace(eos);
  }
  SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
  smartlist_free(tokens);
  if (area) {
    DUMP_AREA(area, "authority cert");
    memarea_drop_all(area);
  }
  return cert;
 err:
  dump_desc(s_dup, "authority cert");
  authority_cert_free(cert);
  SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
  smartlist_free(tokens);
  if (area) {
    DUMP_AREA(area, "authority cert");
    memarea_drop_all(area);
  }
  return NULL;
}

/** Helper: given a string <b>s</b>, return the start of the next router-status
 * object (starting with "r " at the start of a line).  If none is found,
 * return the start of the directory footer, or the next directory signature.
 * If none is found, return the end of the string. */
static inline const char *
find_start_of_next_routerstatus(const char *s)
{
  const char *eos, *footer, *sig;
  if ((eos = strstr(s, "\nr ")))
    ++eos;
  else
    eos = s + strlen(s);

  footer = tor_memstr(s, eos-s, "\ndirectory-footer");
  sig = tor_memstr(s, eos-s, "\ndirectory-signature");

  if (footer && sig)
    return MIN(footer, sig) + 1;
  else if (footer)
    return footer+1;
  else if (sig)
    return sig+1;
  else
    return eos;
}

/** Parse the GuardFraction string from a consensus or vote.
 *
 *  If <b>vote</b> or <b>vote_rs</b> are set the document getting
 *  parsed is a vote routerstatus. Otherwise it's a consensus. This is
 *  the same semantic as in routerstatus_parse_entry_from_string(). */
STATIC int
routerstatus_parse_guardfraction(const char *guardfraction_str,
                                 networkstatus_t *vote,
                                 vote_routerstatus_t *vote_rs,
                                 routerstatus_t *rs)
{
  int ok;
  const char *end_of_header = NULL;
  int is_consensus = !vote_rs;
  uint32_t guardfraction;

  tor_assert(bool_eq(vote, vote_rs));

  /* If this info comes from a consensus, but we should't apply
     guardfraction, just exit. */
  if (is_consensus && !should_apply_guardfraction(NULL)) {
    return 0;
  }

  end_of_header = strchr(guardfraction_str, '=');
  if (!end_of_header) {
    return -1;
  }

  guardfraction = (uint32_t)tor_parse_ulong(end_of_header+1,
                                            10, 0, 100, &ok, NULL);
  if (!ok) {
    log_warn(LD_DIR, "Invalid GuardFraction %s", escaped(guardfraction_str));
    return -1;
  }

  log_debug(LD_GENERAL, "[*] Parsed %s guardfraction '%s' for '%s'.",
            is_consensus ? "consensus" : "vote",
            guardfraction_str, rs->nickname);

  if (!is_consensus) { /* We are parsing a vote */
    vote_rs->status.guardfraction_percentage = guardfraction;
    vote_rs->status.has_guardfraction = 1;
  } else {
    /* We are parsing a consensus. Only apply guardfraction to guards. */
    if (rs->is_possible_guard) {
      rs->guardfraction_percentage = guardfraction;
      rs->has_guardfraction = 1;
    } else {
      log_warn(LD_BUG, "Got GuardFraction for non-guard %s. "
               "This is not supposed to happen. Not applying. ", rs->nickname);
    }
  }

  return 0;
}

/** Given a string at *<b>s</b>, containing a routerstatus object, and an
 * empty smartlist at <b>tokens</b>, parse and return the first router status
 * object in the string, and advance *<b>s</b> to just after the end of the
 * router status.  Return NULL and advance *<b>s</b> on error.
 *
 * If <b>vote</b> and <b>vote_rs</b> are provided, don't allocate a fresh
 * routerstatus but use <b>vote_rs</b> instead.
 *
 * If <b>consensus_method</b> is nonzero, this routerstatus is part of a
 * consensus, and we should parse it according to the method used to
 * make that consensus.
 *
 * Parse according to the syntax used by the consensus flavor <b>flav</b>.
 **/
STATIC routerstatus_t *
routerstatus_parse_entry_from_string(memarea_t *area,
                                     const char **s, smartlist_t *tokens,
                                     networkstatus_t *vote,
                                     vote_routerstatus_t *vote_rs,
                                     int consensus_method,
                                     consensus_flavor_t flav)
{
  const char *eos, *s_dup = *s;
  routerstatus_t *rs = NULL;
  directory_token_t *tok;
  char timebuf[ISO_TIME_LEN+1];
  struct in_addr in;
  int offset = 0;
  tor_assert(tokens);
  tor_assert(bool_eq(vote, vote_rs));

  if (!consensus_method)
    flav = FLAV_NS;
  tor_assert(flav == FLAV_NS || flav == FLAV_MICRODESC);

  eos = find_start_of_next_routerstatus(*s);

  if (tokenize_string(area,*s, eos, tokens, rtrstatus_token_table,0)) {
    log_warn(LD_DIR, "Error tokenizing router status");
    goto err;
  }
  if (smartlist_len(tokens) < 1) {
    log_warn(LD_DIR, "Impossibly short router status");
    goto err;
  }
  tok = find_by_keyword(tokens, K_R);
  tor_assert(tok->n_args >= 7); /* guaranteed by GE(7) in K_R setup */
  if (flav == FLAV_NS) {
    if (tok->n_args < 8) {
      log_warn(LD_DIR, "Too few arguments to r");
      goto err;
    }
  } else if (flav == FLAV_MICRODESC) {
    offset = -1; /* There is no identity digest */
  }

  if (vote_rs) {
    rs = &vote_rs->status;
  } else {
    rs = tor_malloc_zero(sizeof(routerstatus_t));
  }

  if (!is_legal_nickname(tok->args[0])) {
    log_warn(LD_DIR,
             "Invalid nickname %s in router status; skipping.",
             escaped(tok->args[0]));
    goto err;
  }
  strlcpy(rs->nickname, tok->args[0], sizeof(rs->nickname));

  if (digest_from_base64(rs->identity_digest, tok->args[1])) {
    log_warn(LD_DIR, "Error decoding identity digest %s",
             escaped(tok->args[1]));
    goto err;
  }

  if (flav == FLAV_NS) {
    if (digest_from_base64(rs->descriptor_digest, tok->args[2])) {
      log_warn(LD_DIR, "Error decoding descriptor digest %s",
               escaped(tok->args[2]));
      goto err;
    }
  }

  if (tor_snprintf(timebuf, sizeof(timebuf), "%s %s",
                   tok->args[3+offset], tok->args[4+offset]) < 0 ||
      parse_iso_time(timebuf, &rs->published_on)<0) {
    log_warn(LD_DIR, "Error parsing time '%s %s' [%d %d]",
             tok->args[3+offset], tok->args[4+offset],
             offset, (int)flav);
    goto err;
  }

  if (tor_inet_aton(tok->args[5+offset], &in) == 0) {
    log_warn(LD_DIR, "Error parsing router address in network-status %s",
             escaped(tok->args[5+offset]));
    goto err;
  }
  rs->addr = ntohl(in.s_addr);

  rs->or_port = (uint16_t) tor_parse_long(tok->args[6+offset],
                                         10,0,65535,NULL,NULL);
  rs->dir_port = (uint16_t) tor_parse_long(tok->args[7+offset],
                                           10,0,65535,NULL,NULL);

  {
    smartlist_t *a_lines = find_all_by_keyword(tokens, K_A);
    if (a_lines) {
      find_single_ipv6_orport(a_lines, &rs->ipv6_addr, &rs->ipv6_orport);
      smartlist_free(a_lines);
    }
  }

  tok = find_opt_by_keyword(tokens, K_S);
  if (tok && vote) {
    int i;
    vote_rs->flags = 0;
    for (i=0; i < tok->n_args; ++i) {
      int p = smartlist_string_pos(vote->known_flags, tok->args[i]);
      if (p >= 0) {
        vote_rs->flags |= (U64_LITERAL(1)<<p);
      } else {
        log_warn(LD_DIR, "Flags line had a flag %s not listed in known_flags.",
                 escaped(tok->args[i]));
        goto err;
      }
    }
  } else if (tok) {
    /* This is a consensus, not a vote. */
    int i;
    for (i=0; i < tok->n_args; ++i) {
      if (!strcmp(tok->args[i], "Exit"))
        rs->is_exit = 1;
      else if (!strcmp(tok->args[i], "Stable"))
        rs->is_stable = 1;
      else if (!strcmp(tok->args[i], "Fast"))
        rs->is_fast = 1;
      else if (!strcmp(tok->args[i], "Running"))
        rs->is_flagged_running = 1;
      else if (!strcmp(tok->args[i], "Named"))
        rs->is_named = 1;
      else if (!strcmp(tok->args[i], "Valid"))
        rs->is_valid = 1;
      else if (!strcmp(tok->args[i], "Guard"))
        rs->is_possible_guard = 1;
      else if (!strcmp(tok->args[i], "BadExit"))
        rs->is_bad_exit = 1;
      else if (!strcmp(tok->args[i], "Authority"))
        rs->is_authority = 1;
      else if (!strcmp(tok->args[i], "Unnamed") &&
               consensus_method >= 2) {
        /* Unnamed is computed right by consensus method 2 and later. */
        rs->is_unnamed = 1;
      } else if (!strcmp(tok->args[i], "HSDir")) {
        rs->is_hs_dir = 1;
      } else if (!strcmp(tok->args[i], "V2Dir")) {
        rs->is_v2_dir = 1;
      }
    }
    /* These are implied true by having been included in a consensus made
     * with a given method */
    rs->is_flagged_running = 1; /* Starting with consensus method 4. */
    if (consensus_method >= MIN_METHOD_FOR_EXCLUDING_INVALID_NODES)
      rs->is_valid = 1;
  }
  int found_protocol_list = 0;
  if ((tok = find_opt_by_keyword(tokens, K_PROTO))) {
    found_protocol_list = 1;
    rs->protocols_known = 1;
    rs->supports_extend2_cells =
      protocol_list_supports_protocol(tok->args[0], PRT_RELAY, 2);
  }
  if ((tok = find_opt_by_keyword(tokens, K_V))) {
    tor_assert(tok->n_args == 1);
    if (!strcmpstart(tok->args[0], "Tor ") && !found_protocol_list) {
      /* We only do version checks like this in the case where
       * the version is a "Tor" version, and where there is no
       * list of protocol versions that we should be looking at instead. */
      rs->supports_extend2_cells =
        tor_version_as_new_as(tok->args[0], "0.2.4.8-alpha");
      rs->protocols_known = 1;
    }
    if (vote_rs) {
      vote_rs->version = tor_strdup(tok->args[0]);
    }
  }

  /* handle weighting/bandwidth info */
  if ((tok = find_opt_by_keyword(tokens, K_W))) {
    int i;
    for (i=0; i < tok->n_args; ++i) {
      if (!strcmpstart(tok->args[i], "Bandwidth=")) {
        int ok;
        rs->bandwidth_kb =
          (uint32_t)tor_parse_ulong(strchr(tok->args[i], '=')+1,
                                    10, 0, UINT32_MAX,
                                    &ok, NULL);
        if (!ok) {
          log_warn(LD_DIR, "Invalid Bandwidth %s", escaped(tok->args[i]));
          goto err;
        }
        rs->has_bandwidth = 1;
      } else if (!strcmpstart(tok->args[i], "Measured=") && vote_rs) {
        int ok;
        vote_rs->measured_bw_kb =
            (uint32_t)tor_parse_ulong(strchr(tok->args[i], '=')+1,
                                      10, 0, UINT32_MAX, &ok, NULL);
        if (!ok) {
          log_warn(LD_DIR, "Invalid Measured Bandwidth %s",
                   escaped(tok->args[i]));
          goto err;
        }
        vote_rs->has_measured_bw = 1;
        vote->has_measured_bws = 1;
      } else if (!strcmpstart(tok->args[i], "Unmeasured=1")) {
        rs->bw_is_unmeasured = 1;
      } else if (!strcmpstart(tok->args[i], "GuardFraction=")) {
        if (routerstatus_parse_guardfraction(tok->args[i],
                                             vote, vote_rs, rs) < 0) {
          goto err;
        }
      }
    }
  }

  /* parse exit policy summaries */
  if ((tok = find_opt_by_keyword(tokens, K_P))) {
    tor_assert(tok->n_args == 1);
    if (strcmpstart(tok->args[0], "accept ") &&
        strcmpstart(tok->args[0], "reject ")) {
      log_warn(LD_DIR, "Unknown exit policy summary type %s.",
               escaped(tok->args[0]));
      goto err;
    }
    /* XXX weasel: parse this into ports and represent them somehow smart,
     * maybe not here but somewhere on if we need it for the client.
     * we should still parse it here to check it's valid tho.
     */
    rs->exitsummary = tor_strdup(tok->args[0]);
    rs->has_exitsummary = 1;
  }

  if (vote_rs) {
    SMARTLIST_FOREACH_BEGIN(tokens, directory_token_t *, t) {
      if (t->tp == K_M && t->n_args) {
        vote_microdesc_hash_t *line =
          tor_malloc(sizeof(vote_microdesc_hash_t));
        line->next = vote_rs->microdesc;
        line->microdesc_hash_line = tor_strdup(t->args[0]);
        vote_rs->microdesc = line;
      }
      if (t->tp == K_ID) {
        tor_assert(t->n_args >= 2);
        if (!strcmp(t->args[0], "ed25519")) {
          vote_rs->has_ed25519_listing = 1;
          if (strcmp(t->args[1], "none") &&
              digest256_from_base64((char*)vote_rs->ed25519_id,
                                    t->args[1])<0) {
            log_warn(LD_DIR, "Bogus ed25519 key in networkstatus vote");
            goto err;
          }
        }
      }
      if (t->tp == K_PROTO) {
        tor_assert(t->n_args == 1);
        vote_rs->protocols = tor_strdup(t->args[0]);
      }
    } SMARTLIST_FOREACH_END(t);
  } else if (flav == FLAV_MICRODESC) {
    tok = find_opt_by_keyword(tokens, K_M);
    if (tok) {
      tor_assert(tok->n_args);
      if (digest256_from_base64(rs->descriptor_digest, tok->args[0])) {
        log_warn(LD_DIR, "Error decoding microdescriptor digest %s",
                 escaped(tok->args[0]));
        goto err;
      }
    } else {
      log_info(LD_BUG, "Found an entry in networkstatus with no "
               "microdescriptor digest. (Router %s ($%s) at %s:%d.)",
               rs->nickname, hex_str(rs->identity_digest, DIGEST_LEN),
               fmt_addr32(rs->addr), rs->or_port);
    }
  }

  if (!strcasecmp(rs->nickname, UNNAMED_ROUTER_NICKNAME))
    rs->is_named = 0;

  goto done;
 err:
  dump_desc(s_dup, "routerstatus entry");
  if (rs && !vote_rs)
    routerstatus_free(rs);
  rs = NULL;
 done:
  SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
  smartlist_clear(tokens);
  if (area) {
    DUMP_AREA(area, "routerstatus entry");
    memarea_clear(area);
  }
  *s = eos;

  return rs;
}

int
compare_vote_routerstatus_entries(const void **_a, const void **_b)
{
  const vote_routerstatus_t *a = *_a, *b = *_b;
  return fast_memcmp(a->status.identity_digest, b->status.identity_digest,
                     DIGEST_LEN);
}

/** Verify the bandwidth weights of a network status document */
int
networkstatus_verify_bw_weights(networkstatus_t *ns, int consensus_method)
{
  int64_t weight_scale;
  int64_t G=0, M=0, E=0, D=0, T=0;
  double Wgg, Wgm, Wgd, Wmg, Wmm, Wme, Wmd, Weg, Wem, Wee, Wed;
  double Gtotal=0, Mtotal=0, Etotal=0;
  const char *casename = NULL;
  int valid = 1;
  (void) consensus_method;

  weight_scale = networkstatus_get_weight_scale_param(ns);
  Wgg = networkstatus_get_bw_weight(ns, "Wgg", -1);
  Wgm = networkstatus_get_bw_weight(ns, "Wgm", -1);
  Wgd = networkstatus_get_bw_weight(ns, "Wgd", -1);
  Wmg = networkstatus_get_bw_weight(ns, "Wmg", -1);
  Wmm = networkstatus_get_bw_weight(ns, "Wmm", -1);
  Wme = networkstatus_get_bw_weight(ns, "Wme", -1);
  Wmd = networkstatus_get_bw_weight(ns, "Wmd", -1);
  Weg = networkstatus_get_bw_weight(ns, "Weg", -1);
  Wem = networkstatus_get_bw_weight(ns, "Wem", -1);
  Wee = networkstatus_get_bw_weight(ns, "Wee", -1);
  Wed = networkstatus_get_bw_weight(ns, "Wed", -1);

  if (Wgg<0 || Wgm<0 || Wgd<0 || Wmg<0 || Wmm<0 || Wme<0 || Wmd<0 || Weg<0
          || Wem<0 || Wee<0 || Wed<0) {
    log_warn(LD_BUG, "No bandwidth weights produced in consensus!");
    return 0;
  }

  // First, sanity check basic summing properties that hold for all cases
  // We use > 1 as the check for these because they are computed as integers.
  // Sometimes there are rounding errors.
  if (fabs(Wmm - weight_scale) > 1) {
    log_warn(LD_BUG, "Wmm=%f != "I64_FORMAT,
             Wmm, I64_PRINTF_ARG(weight_scale));
    valid = 0;
  }

  if (fabs(Wem - Wee) > 1) {
    log_warn(LD_BUG, "Wem=%f != Wee=%f", Wem, Wee);
    valid = 0;
  }

  if (fabs(Wgm - Wgg) > 1) {
    log_warn(LD_BUG, "Wgm=%f != Wgg=%f", Wgm, Wgg);
    valid = 0;
  }

  if (fabs(Weg - Wed) > 1) {
    log_warn(LD_BUG, "Wed=%f != Weg=%f", Wed, Weg);
    valid = 0;
  }

  if (fabs(Wgg + Wmg - weight_scale) > 0.001*weight_scale) {
    log_warn(LD_BUG, "Wgg=%f != "I64_FORMAT" - Wmg=%f", Wgg,
             I64_PRINTF_ARG(weight_scale), Wmg);
    valid = 0;
  }

  if (fabs(Wee + Wme - weight_scale) > 0.001*weight_scale) {
    log_warn(LD_BUG, "Wee=%f != "I64_FORMAT" - Wme=%f", Wee,
             I64_PRINTF_ARG(weight_scale), Wme);
    valid = 0;
  }

  if (fabs(Wgd + Wmd + Wed - weight_scale) > 0.001*weight_scale) {
    log_warn(LD_BUG, "Wgd=%f + Wmd=%f + Wed=%f != "I64_FORMAT,
             Wgd, Wmd, Wed, I64_PRINTF_ARG(weight_scale));
    valid = 0;
  }

  Wgg /= weight_scale;
  Wgm /= weight_scale;
  Wgd /= weight_scale;

  Wmg /= weight_scale;
  Wmm /= weight_scale;
  Wme /= weight_scale;
  Wmd /= weight_scale;

  Weg /= weight_scale;
  Wem /= weight_scale;
  Wee /= weight_scale;
  Wed /= weight_scale;

  // Then, gather G, M, E, D, T to determine case
  SMARTLIST_FOREACH_BEGIN(ns->routerstatus_list, routerstatus_t *, rs) {
    int is_exit = 0;
    /* Bug #2203: Don't count bad exits as exits for balancing */
    is_exit = rs->is_exit && !rs->is_bad_exit;
    if (rs->has_bandwidth) {
      T += rs->bandwidth_kb;
      if (is_exit && rs->is_possible_guard) {
        D += rs->bandwidth_kb;
        Gtotal += Wgd*rs->bandwidth_kb;
        Mtotal += Wmd*rs->bandwidth_kb;
        Etotal += Wed*rs->bandwidth_kb;
      } else if (is_exit) {
        E += rs->bandwidth_kb;
        Mtotal += Wme*rs->bandwidth_kb;
        Etotal += Wee*rs->bandwidth_kb;
      } else if (rs->is_possible_guard) {
        G += rs->bandwidth_kb;
        Gtotal += Wgg*rs->bandwidth_kb;
        Mtotal += Wmg*rs->bandwidth_kb;
      } else {
        M += rs->bandwidth_kb;
        Mtotal += Wmm*rs->bandwidth_kb;
      }
    } else {
      log_warn(LD_BUG, "Missing consensus bandwidth for router %s",
               routerstatus_describe(rs));
    }
  } SMARTLIST_FOREACH_END(rs);

  // Finally, check equality conditions depending upon case 1, 2 or 3
  // Full equality cases: 1, 3b
  // Partial equality cases: 2b (E=G), 3a (M=E)
  // Fully unknown: 2a
  if (3*E >= T && 3*G >= T) {
    // Case 1: Neither are scarce
    casename = "Case 1";
    if (fabs(Etotal-Mtotal) > 0.01*MAX(Etotal,Mtotal)) {
      log_warn(LD_DIR,
               "Bw Weight Failure for %s: Etotal %f != Mtotal %f. "
               "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
               " T="I64_FORMAT". "
               "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
               casename, Etotal, Mtotal,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
               Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
      valid = 0;
    }
    if (fabs(Etotal-Gtotal) > 0.01*MAX(Etotal,Gtotal)) {
      log_warn(LD_DIR,
               "Bw Weight Failure for %s: Etotal %f != Gtotal %f. "
               "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
               " T="I64_FORMAT". "
               "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
               casename, Etotal, Gtotal,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
               Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
      valid = 0;
    }
    if (fabs(Gtotal-Mtotal) > 0.01*MAX(Gtotal,Mtotal)) {
      log_warn(LD_DIR,
               "Bw Weight Failure for %s: Mtotal %f != Gtotal %f. "
               "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
               " T="I64_FORMAT". "
               "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
               casename, Mtotal, Gtotal,
               I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
               I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
               Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
      valid = 0;
    }
  } else if (3*E < T && 3*G < T) {
    int64_t R = MIN(E, G);
    int64_t S = MAX(E, G);
    /*
     * Case 2: Both Guards and Exits are scarce
     * Balance D between E and G, depending upon
     * D capacity and scarcity. Devote no extra
     * bandwidth to middle nodes.
     */
    if (R+D < S) { // Subcase a
      double Rtotal, Stotal;
      if (E < G) {
        Rtotal = Etotal;
        Stotal = Gtotal;
      } else {
        Rtotal = Gtotal;
        Stotal = Etotal;
      }
      casename = "Case 2a";
      // Rtotal < Stotal
      if (Rtotal > Stotal) {
        log_warn(LD_DIR,
                   "Bw Weight Failure for %s: Rtotal %f > Stotal %f. "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Rtotal, Stotal,
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
      // Rtotal < T/3
      if (3*Rtotal > T) {
        log_warn(LD_DIR,
                   "Bw Weight Failure for %s: 3*Rtotal %f > T "
                   I64_FORMAT". G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                   " D="I64_FORMAT" T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Rtotal*3, I64_PRINTF_ARG(T),
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
      // Stotal < T/3
      if (3*Stotal > T) {
        log_warn(LD_DIR,
                   "Bw Weight Failure for %s: 3*Stotal %f > T "
                   I64_FORMAT". G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                   " D="I64_FORMAT" T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Stotal*3, I64_PRINTF_ARG(T),
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
      // Mtotal > T/3
      if (3*Mtotal < T) {
        log_warn(LD_DIR,
                   "Bw Weight Failure for %s: 3*Mtotal %f < T "
                   I64_FORMAT". "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Mtotal*3, I64_PRINTF_ARG(T),
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
    } else { // Subcase b: R+D > S
      casename = "Case 2b";

      /* Check the rare-M redirect case. */
      if (D != 0 && 3*M < T) {
        casename = "Case 2b (balanced)";
        if (fabs(Etotal-Mtotal) > 0.01*MAX(Etotal,Mtotal)) {
          log_warn(LD_DIR,
                   "Bw Weight Failure for %s: Etotal %f != Mtotal %f. "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Etotal, Mtotal,
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
          valid = 0;
        }
        if (fabs(Etotal-Gtotal) > 0.01*MAX(Etotal,Gtotal)) {
          log_warn(LD_DIR,
                   "Bw Weight Failure for %s: Etotal %f != Gtotal %f. "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Etotal, Gtotal,
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
          valid = 0;
        }
        if (fabs(Gtotal-Mtotal) > 0.01*MAX(Gtotal,Mtotal)) {
          log_warn(LD_DIR,
                   "Bw Weight Failure for %s: Mtotal %f != Gtotal %f. "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Mtotal, Gtotal,
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
          valid = 0;
        }
      } else {
        if (fabs(Etotal-Gtotal) > 0.01*MAX(Etotal,Gtotal)) {
          log_warn(LD_DIR,
                   "Bw Weight Failure for %s: Etotal %f != Gtotal %f. "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Etotal, Gtotal,
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
          valid = 0;
        }
      }
    }
  } else { // if (E < T/3 || G < T/3) {
    int64_t S = MIN(E, G);
    int64_t NS = MAX(E, G);
    if (3*(S+D) < T) { // Subcase a:
      double Stotal;
      double NStotal;
      if (G < E) {
        casename = "Case 3a (G scarce)";
        Stotal = Gtotal;
        NStotal = Etotal;
      } else { // if (G >= E) {
        casename = "Case 3a (E scarce)";
        NStotal = Gtotal;
        Stotal = Etotal;
      }
      // Stotal < T/3
      if (3*Stotal > T) {
        log_warn(LD_DIR,
                   "Bw Weight Failure for %s: 3*Stotal %f > T "
                   I64_FORMAT". G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT
                   " D="I64_FORMAT" T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, Stotal*3, I64_PRINTF_ARG(T),
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
      if (NS >= M) {
        if (fabs(NStotal-Mtotal) > 0.01*MAX(NStotal,Mtotal)) {
          log_warn(LD_DIR,
                   "Bw Weight Failure for %s: NStotal %f != Mtotal %f. "
                   "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                   " T="I64_FORMAT". "
                   "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                   casename, NStotal, Mtotal,
                   I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                   I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                   Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
          valid = 0;
        }
      } else {
        // if NS < M, NStotal > T/3 because only one of G or E is scarce
        if (3*NStotal < T) {
          log_warn(LD_DIR,
                     "Bw Weight Failure for %s: 3*NStotal %f < T "
                     I64_FORMAT". G="I64_FORMAT" M="I64_FORMAT
                     " E="I64_FORMAT" D="I64_FORMAT" T="I64_FORMAT". "
                     "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                     casename, NStotal*3, I64_PRINTF_ARG(T),
                     I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                     I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                     Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
          valid = 0;
        }
      }
    } else { // Subcase b: S+D >= T/3
      casename = "Case 3b";
      if (fabs(Etotal-Mtotal) > 0.01*MAX(Etotal,Mtotal)) {
        log_warn(LD_DIR,
                 "Bw Weight Failure for %s: Etotal %f != Mtotal %f. "
                 "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                 " T="I64_FORMAT". "
                 "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                 casename, Etotal, Mtotal,
                 I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                 I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                 Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
      if (fabs(Etotal-Gtotal) > 0.01*MAX(Etotal,Gtotal)) {
        log_warn(LD_DIR,
                 "Bw Weight Failure for %s: Etotal %f != Gtotal %f. "
                 "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                 " T="I64_FORMAT". "
                 "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                 casename, Etotal, Gtotal,
                 I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                 I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                 Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
      if (fabs(Gtotal-Mtotal) > 0.01*MAX(Gtotal,Mtotal)) {
        log_warn(LD_DIR,
                 "Bw Weight Failure for %s: Mtotal %f != Gtotal %f. "
                 "G="I64_FORMAT" M="I64_FORMAT" E="I64_FORMAT" D="I64_FORMAT
                 " T="I64_FORMAT". "
                 "Wgg=%f Wgd=%f Wmg=%f Wme=%f Wmd=%f Wee=%f Wed=%f",
                 casename, Mtotal, Gtotal,
                 I64_PRINTF_ARG(G), I64_PRINTF_ARG(M), I64_PRINTF_ARG(E),
                 I64_PRINTF_ARG(D), I64_PRINTF_ARG(T),
                 Wgg, Wgd, Wmg, Wme, Wmd, Wee, Wed);
        valid = 0;
      }
    }
  }

  if (valid)
    log_notice(LD_DIR, "Bandwidth-weight %s is verified and valid.",
               casename);

  return valid;
}

/** Parse and extract all SR commits from <b>tokens</b> and place them in
 *  <b>ns</b>. */
static void
extract_shared_random_commits(networkstatus_t *ns, smartlist_t *tokens)
{
  smartlist_t *chunks = NULL;

  tor_assert(ns);
  tor_assert(tokens);
  /* Commits are only present in a vote. */
  tor_assert(ns->type == NS_TYPE_VOTE);

  ns->sr_info.commits = smartlist_new();

  smartlist_t *commits = find_all_by_keyword(tokens, K_COMMIT);
  /* It's normal that a vote might contain no commits even if it participates
   * in the SR protocol. Don't treat it as an error. */
  if (commits == NULL) {
    goto end;
  }

  /* Parse the commit. We do NO validation of number of arguments or ordering
   * for forward compatibility, it's the parse commit job to inform us if it's
   * supported or not. */
  chunks = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(commits, directory_token_t *, tok) {
    /* Extract all arguments and put them in the chunks list. */
    for (int i = 0; i < tok->n_args; i++) {
      smartlist_add(chunks, tok->args[i]);
    }
    sr_commit_t *commit = sr_parse_commit(chunks);
    smartlist_clear(chunks);
    if (commit == NULL) {
      /* Get voter identity so we can warn that this dirauth vote contains
       * commit we can't parse. */
      networkstatus_voter_info_t *voter = smartlist_get(ns->voters, 0);
      tor_assert(voter);
      log_warn(LD_DIR, "SR: Unable to parse commit %s from vote of voter %s.",
               escaped(tok->object_body),
               hex_str(voter->identity_digest,
                       sizeof(voter->identity_digest)));
      /* Commitment couldn't be parsed. Continue onto the next commit because
       * this one could be unsupported for instance. */
      continue;
    }
    /* Add newly created commit object to the vote. */
    smartlist_add(ns->sr_info.commits, commit);
  } SMARTLIST_FOREACH_END(tok);

 end:
  smartlist_free(chunks);
  smartlist_free(commits);
}

/** Check if a shared random value of type <b>srv_type</b> is in
 *  <b>tokens</b>. If there is, parse it and set it to <b>srv_out</b>. Return
 *  -1 on failure, 0 on success. The resulting srv is allocated on the heap and
 *  it's the responsibility of the caller to free it. */
static int
extract_one_srv(smartlist_t *tokens, directory_keyword srv_type,
                sr_srv_t **srv_out)
{
  int ret = -1;
  directory_token_t *tok;
  sr_srv_t *srv = NULL;
  smartlist_t *chunks;

  tor_assert(tokens);

  chunks = smartlist_new();
  tok = find_opt_by_keyword(tokens, srv_type);
  if (!tok) {
    /* That's fine, no SRV is allowed. */
    ret = 0;
    goto end;
  }
  for (int i = 0; i < tok->n_args; i++) {
    smartlist_add(chunks, tok->args[i]);
  }
  srv = sr_parse_srv(chunks);
  if (srv == NULL) {
    log_warn(LD_DIR, "SR: Unparseable SRV %s", escaped(tok->object_body));
    goto end;
  }
  /* All is good. */
  *srv_out = srv;
  ret = 0;
 end:
  smartlist_free(chunks);
  return ret;
}

/** Extract any shared random values found in <b>tokens</b> and place them in
 *  the networkstatus <b>ns</b>. */
static void
extract_shared_random_srvs(networkstatus_t *ns, smartlist_t *tokens)
{
  const char *voter_identity;
  networkstatus_voter_info_t *voter;

  tor_assert(ns);
  tor_assert(tokens);
  /* Can be only one of them else code flow. */
  tor_assert(ns->type == NS_TYPE_VOTE || ns->type == NS_TYPE_CONSENSUS);

  if (ns->type == NS_TYPE_VOTE) {
    voter = smartlist_get(ns->voters, 0);
    tor_assert(voter);
    voter_identity = hex_str(voter->identity_digest,
                             sizeof(voter->identity_digest));
  } else {
    /* Consensus has multiple voters so no specific voter. */
    voter_identity = "consensus";
  }

  /* We extract both and on error, everything is stopped because it means
   * the votes is malformed for the shared random value(s). */
  if (extract_one_srv(tokens, K_PREVIOUS_SRV, &ns->sr_info.previous_srv) < 0) {
    log_warn(LD_DIR, "SR: Unable to parse previous SRV from %s",
             voter_identity);
    /* Maybe we have a chance with the current SRV so let's try it anyway. */
  }
  if (extract_one_srv(tokens, K_CURRENT_SRV, &ns->sr_info.current_srv) < 0) {
    log_warn(LD_DIR, "SR: Unable to parse current SRV from %s",
             voter_identity);
  }
}

/** Parse a v3 networkstatus vote, opinion, or consensus (depending on
 * ns_type), from <b>s</b>, and return the result.  Return NULL on failure. */
networkstatus_t *
networkstatus_parse_vote_from_string(const char *s, const char **eos_out,
                                     networkstatus_type_t ns_type)
{
  smartlist_t *tokens = smartlist_new();
  smartlist_t *rs_tokens = NULL, *footer_tokens = NULL;
  networkstatus_voter_info_t *voter = NULL;
  networkstatus_t *ns = NULL;
  common_digests_t ns_digests;
  const char *cert, *end_of_header, *end_of_footer, *s_dup = s;
  directory_token_t *tok;
  struct in_addr in;
  int i, inorder, n_signatures = 0;
  memarea_t *area = NULL, *rs_area = NULL;
  consensus_flavor_t flav = FLAV_NS;
  char *last_kwd=NULL;

  tor_assert(s);

  if (eos_out)
    *eos_out = NULL;

  if (router_get_networkstatus_v3_hashes(s, &ns_digests)) {
    log_warn(LD_DIR, "Unable to compute digest of network-status");
    goto err;
  }

  area = memarea_new();
  end_of_header = find_start_of_next_routerstatus(s);
  if (tokenize_string(area, s, end_of_header, tokens,
                      (ns_type == NS_TYPE_CONSENSUS) ?
                      networkstatus_consensus_token_table :
                      networkstatus_token_table, 0)) {
    log_warn(LD_DIR, "Error tokenizing network-status header");
    goto err;
  }

  ns = tor_malloc_zero(sizeof(networkstatus_t));
  memcpy(&ns->digests, &ns_digests, sizeof(ns_digests));

  tok = find_by_keyword(tokens, K_NETWORK_STATUS_VERSION);
  tor_assert(tok);
  if (tok->n_args > 1) {
    int flavor = networkstatus_parse_flavor_name(tok->args[1]);
    if (flavor < 0) {
      log_warn(LD_DIR, "Can't parse document with unknown flavor %s",
               escaped(tok->args[1]));
      goto err;
    }
    ns->flavor = flav = flavor;
  }
  if (flav != FLAV_NS && ns_type != NS_TYPE_CONSENSUS) {
    log_warn(LD_DIR, "Flavor found on non-consensus networkstatus.");
    goto err;
  }

  if (ns_type != NS_TYPE_CONSENSUS) {
    const char *end_of_cert = NULL;
    if (!(cert = strstr(s, "\ndir-key-certificate-version")))
      goto err;
    ++cert;
    ns->cert = authority_cert_parse_from_string(cert, &end_of_cert);
    if (!ns->cert || !end_of_cert || end_of_cert > end_of_header)
      goto err;
  }

  tok = find_by_keyword(tokens, K_VOTE_STATUS);
  tor_assert(tok->n_args);
  if (!strcmp(tok->args[0], "vote")) {
    ns->type = NS_TYPE_VOTE;
  } else if (!strcmp(tok->args[0], "consensus")) {
    ns->type = NS_TYPE_CONSENSUS;
  } else if (!strcmp(tok->args[0], "opinion")) {
    ns->type = NS_TYPE_OPINION;
  } else {
    log_warn(LD_DIR, "Unrecognized vote status %s in network-status",
             escaped(tok->args[0]));
    goto err;
  }
  if (ns_type != ns->type) {
    log_warn(LD_DIR, "Got the wrong kind of v3 networkstatus.");
    goto err;
  }

  if (ns->type == NS_TYPE_VOTE || ns->type == NS_TYPE_OPINION) {
    tok = find_by_keyword(tokens, K_PUBLISHED);
    if (parse_iso_time(tok->args[0], &ns->published))
      goto err;

    ns->supported_methods = smartlist_new();
    tok = find_opt_by_keyword(tokens, K_CONSENSUS_METHODS);
    if (tok) {
      for (i=0; i < tok->n_args; ++i)
        smartlist_add(ns->supported_methods, tor_strdup(tok->args[i]));
    } else {
      smartlist_add(ns->supported_methods, tor_strdup("1"));
    }
  } else {
    tok = find_opt_by_keyword(tokens, K_CONSENSUS_METHOD);
    if (tok) {
      int num_ok;
      ns->consensus_method = (int)tor_parse_long(tok->args[0], 10, 1, INT_MAX,
                                                 &num_ok, NULL);
      if (!num_ok)
        goto err;
    } else {
      ns->consensus_method = 1;
    }
  }

  if ((tok = find_opt_by_keyword(tokens, K_RECOMMENDED_CLIENT_PROTOCOLS)))
    ns->recommended_client_protocols = tor_strdup(tok->args[0]);
  if ((tok = find_opt_by_keyword(tokens, K_RECOMMENDED_RELAY_PROTOCOLS)))
    ns->recommended_relay_protocols = tor_strdup(tok->args[0]);
  if ((tok = find_opt_by_keyword(tokens, K_REQUIRED_CLIENT_PROTOCOLS)))
    ns->required_client_protocols = tor_strdup(tok->args[0]);
  if ((tok = find_opt_by_keyword(tokens, K_REQUIRED_RELAY_PROTOCOLS)))
    ns->required_relay_protocols = tor_strdup(tok->args[0]);

  tok = find_by_keyword(tokens, K_VALID_AFTER);
  if (parse_iso_time(tok->args[0], &ns->valid_after))
    goto err;

  tok = find_by_keyword(tokens, K_FRESH_UNTIL);
  if (parse_iso_time(tok->args[0], &ns->fresh_until))
    goto err;

  tok = find_by_keyword(tokens, K_VALID_UNTIL);
  if (parse_iso_time(tok->args[0], &ns->valid_until))
    goto err;

  tok = find_by_keyword(tokens, K_VOTING_DELAY);
  tor_assert(tok->n_args >= 2);
  {
    int ok;
    ns->vote_seconds =
      (int) tor_parse_long(tok->args[0], 10, 0, INT_MAX, &ok, NULL);
    if (!ok)
      goto err;
    ns->dist_seconds =
      (int) tor_parse_long(tok->args[1], 10, 0, INT_MAX, &ok, NULL);
    if (!ok)
      goto err;
  }
  if (ns->valid_after +
      (get_options()->TestingTorNetwork ?
       MIN_VOTE_INTERVAL_TESTING : MIN_VOTE_INTERVAL) > ns->fresh_until) {
    log_warn(LD_DIR, "Vote/consensus freshness interval is too short");
    goto err;
  }
  if (ns->valid_after +
      (get_options()->TestingTorNetwork ?
       MIN_VOTE_INTERVAL_TESTING : MIN_VOTE_INTERVAL)*2 > ns->valid_until) {
    log_warn(LD_DIR, "Vote/consensus liveness interval is too short");
    goto err;
  }
  if (ns->vote_seconds < MIN_VOTE_SECONDS) {
    log_warn(LD_DIR, "Vote seconds is too short");
    goto err;
  }
  if (ns->dist_seconds < MIN_DIST_SECONDS) {
    log_warn(LD_DIR, "Dist seconds is too short");
    goto err;
  }

  if ((tok = find_opt_by_keyword(tokens, K_CLIENT_VERSIONS))) {
    ns->client_versions = tor_strdup(tok->args[0]);
  }
  if ((tok = find_opt_by_keyword(tokens, K_SERVER_VERSIONS))) {
    ns->server_versions = tor_strdup(tok->args[0]);
  }

  {
    smartlist_t *package_lst = find_all_by_keyword(tokens, K_PACKAGE);
    ns->package_lines = smartlist_new();
    if (package_lst) {
      SMARTLIST_FOREACH(package_lst, directory_token_t *, t,
                    smartlist_add(ns->package_lines, tor_strdup(t->args[0])));
    }
    smartlist_free(package_lst);
  }

  tok = find_by_keyword(tokens, K_KNOWN_FLAGS);
  ns->known_flags = smartlist_new();
  inorder = 1;
  for (i = 0; i < tok->n_args; ++i) {
    smartlist_add(ns->known_flags, tor_strdup(tok->args[i]));
    if (i>0 && strcmp(tok->args[i-1], tok->args[i])>= 0) {
      log_warn(LD_DIR, "%s >= %s", tok->args[i-1], tok->args[i]);
      inorder = 0;
    }
  }
  if (!inorder) {
    log_warn(LD_DIR, "known-flags not in order");
    goto err;
  }
  if (ns->type != NS_TYPE_CONSENSUS &&
      smartlist_len(ns->known_flags) > MAX_KNOWN_FLAGS_IN_VOTE) {
    /* If we allowed more than 64 flags in votes, then parsing them would make
     * us invoke undefined behavior whenever we used 1<<flagnum to do a
     * bit-shift. This is only for votes and opinions: consensus users don't
     * care about flags they don't recognize, and so don't build a bitfield
     * for them. */
    log_warn(LD_DIR, "Too many known-flags in consensus vote or opinion");
    goto err;
  }

  tok = find_opt_by_keyword(tokens, K_PARAMS);
  if (tok) {
    int any_dups = 0;
    inorder = 1;
    ns->net_params = smartlist_new();
    for (i = 0; i < tok->n_args; ++i) {
      int ok=0;
      char *eq = strchr(tok->args[i], '=');
      size_t eq_pos;
      if (!eq) {
        log_warn(LD_DIR, "Bad element '%s' in params", escaped(tok->args[i]));
        goto err;
      }
      eq_pos = eq-tok->args[i];
      tor_parse_long(eq+1, 10, INT32_MIN, INT32_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_DIR, "Bad element '%s' in params", escaped(tok->args[i]));
        goto err;
      }
      if (i > 0 && strcmp(tok->args[i-1], tok->args[i]) >= 0) {
        log_warn(LD_DIR, "%s >= %s", tok->args[i-1], tok->args[i]);
        inorder = 0;
      }
      if (last_kwd && eq_pos == strlen(last_kwd) &&
          fast_memeq(last_kwd, tok->args[i], eq_pos)) {
        log_warn(LD_DIR, "Duplicate value for %s parameter",
                 escaped(tok->args[i]));
        any_dups = 1;
      }
      tor_free(last_kwd);
      last_kwd = tor_strndup(tok->args[i], eq_pos);
      smartlist_add(ns->net_params, tor_strdup(tok->args[i]));
    }
    if (!inorder) {
      log_warn(LD_DIR, "params not in order");
      goto err;
    }
    if (any_dups) {
      log_warn(LD_DIR, "Duplicate in parameters");
      goto err;
    }
  }

  ns->voters = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(tokens, directory_token_t *, _tok) {
    tok = _tok;
    if (tok->tp == K_DIR_SOURCE) {
      tor_assert(tok->n_args >= 6);

      if (voter)
        smartlist_add(ns->voters, voter);
      voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
      voter->sigs = smartlist_new();
      if (ns->type != NS_TYPE_CONSENSUS)
        memcpy(voter->vote_digest, ns_digests.d[DIGEST_SHA1], DIGEST_LEN);

      voter->nickname = tor_strdup(tok->args[0]);
      if (strlen(tok->args[1]) != HEX_DIGEST_LEN ||
          base16_decode(voter->identity_digest, sizeof(voter->identity_digest),
                        tok->args[1], HEX_DIGEST_LEN)
                        != sizeof(voter->identity_digest)) {
        log_warn(LD_DIR, "Error decoding identity digest %s in "
                 "network-status document.", escaped(tok->args[1]));
        goto err;
      }
      if (ns->type != NS_TYPE_CONSENSUS &&
          tor_memneq(ns->cert->cache_info.identity_digest,
                 voter->identity_digest, DIGEST_LEN)) {
        log_warn(LD_DIR,"Mismatch between identities in certificate and vote");
        goto err;
      }
      if (ns->type != NS_TYPE_CONSENSUS) {
        if (authority_cert_is_blacklisted(ns->cert)) {
          log_warn(LD_DIR, "Rejecting vote signature made with blacklisted "
                   "signing key %s",
                   hex_str(ns->cert->signing_key_digest, DIGEST_LEN));
          goto err;
        }
      }
      voter->address = tor_strdup(tok->args[2]);
      if (!tor_inet_aton(tok->args[3], &in)) {
        log_warn(LD_DIR, "Error decoding IP address %s in network-status.",
                 escaped(tok->args[3]));
        goto err;
      }
      voter->addr = ntohl(in.s_addr);
      int ok;
      voter->dir_port = (uint16_t)
        tor_parse_long(tok->args[4], 10, 0, 65535, &ok, NULL);
      if (!ok)
        goto err;
      voter->or_port = (uint16_t)
        tor_parse_long(tok->args[5], 10, 0, 65535, &ok, NULL);
      if (!ok)
        goto err;
    } else if (tok->tp == K_CONTACT) {
      if (!voter || voter->contact) {
        log_warn(LD_DIR, "contact element is out of place.");
        goto err;
      }
      voter->contact = tor_strdup(tok->args[0]);
    } else if (tok->tp == K_VOTE_DIGEST) {
      tor_assert(ns->type == NS_TYPE_CONSENSUS);
      tor_assert(tok->n_args >= 1);
      if (!voter || ! tor_digest_is_zero(voter->vote_digest)) {
        log_warn(LD_DIR, "vote-digest element is out of place.");
        goto err;
      }
      if (strlen(tok->args[0]) != HEX_DIGEST_LEN ||
        base16_decode(voter->vote_digest, sizeof(voter->vote_digest),
                      tok->args[0], HEX_DIGEST_LEN)
                      != sizeof(voter->vote_digest)) {
        log_warn(LD_DIR, "Error decoding vote digest %s in "
                 "network-status consensus.", escaped(tok->args[0]));
        goto err;
      }
    }
  } SMARTLIST_FOREACH_END(_tok);
  if (voter) {
    smartlist_add(ns->voters, voter);
    voter = NULL;
  }
  if (smartlist_len(ns->voters) == 0) {
    log_warn(LD_DIR, "Missing dir-source elements in a networkstatus.");
    goto err;
  } else if (ns->type != NS_TYPE_CONSENSUS && smartlist_len(ns->voters) != 1) {
    log_warn(LD_DIR, "Too many dir-source elements in a vote networkstatus.");
    goto err;
  }

  if (ns->type != NS_TYPE_CONSENSUS &&
      (tok = find_opt_by_keyword(tokens, K_LEGACY_DIR_KEY))) {
    int bad = 1;
    if (strlen(tok->args[0]) == HEX_DIGEST_LEN) {
      networkstatus_voter_info_t *voter_0 = smartlist_get(ns->voters, 0);
      if (base16_decode(voter_0->legacy_id_digest, DIGEST_LEN,
                        tok->args[0], HEX_DIGEST_LEN) != DIGEST_LEN)
        bad = 1;
      else
        bad = 0;
    }
    if (bad) {
      log_warn(LD_DIR, "Invalid legacy key digest %s on vote.",
               escaped(tok->args[0]));
    }
  }

  /* If this is a vote document, check if information about the shared
     randomness protocol is included, and extract it. */
  if (ns->type == NS_TYPE_VOTE) {
    /* Does this authority participates in the SR protocol? */
    tok = find_opt_by_keyword(tokens, K_SR_FLAG);
    if (tok) {
      ns->sr_info.participate = 1;
      /* Get the SR commitments and reveals from the vote. */
      extract_shared_random_commits(ns, tokens);
    }
  }
  /* For both a vote and consensus, extract the shared random values. */
  if (ns->type == NS_TYPE_VOTE || ns->type == NS_TYPE_CONSENSUS) {
    extract_shared_random_srvs(ns, tokens);
  }

  /* Parse routerstatus lines. */
  rs_tokens = smartlist_new();
  rs_area = memarea_new();
  s = end_of_header;
  ns->routerstatus_list = smartlist_new();

  while (!strcmpstart(s, "r ")) {
    if (ns->type != NS_TYPE_CONSENSUS) {
      vote_routerstatus_t *rs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      if (routerstatus_parse_entry_from_string(rs_area, &s, rs_tokens, ns,
                                               rs, 0, 0))
        smartlist_add(ns->routerstatus_list, rs);
      else {
        tor_free(rs->version);
        tor_free(rs);
      }
    } else {
      routerstatus_t *rs;
      if ((rs = routerstatus_parse_entry_from_string(rs_area, &s, rs_tokens,
                                                     NULL, NULL,
                                                     ns->consensus_method,
                                                     flav))) {
        /* Use exponential-backoff scheduling when downloading microdescs */
        rs->dl_status.backoff = DL_SCHED_RANDOM_EXPONENTIAL;
        smartlist_add(ns->routerstatus_list, rs);
      }
    }
  }
  for (i = 1; i < smartlist_len(ns->routerstatus_list); ++i) {
    routerstatus_t *rs1, *rs2;
    if (ns->type != NS_TYPE_CONSENSUS) {
      vote_routerstatus_t *a = smartlist_get(ns->routerstatus_list, i-1);
      vote_routerstatus_t *b = smartlist_get(ns->routerstatus_list, i);
      rs1 = &a->status; rs2 = &b->status;
    } else {
      rs1 = smartlist_get(ns->routerstatus_list, i-1);
      rs2 = smartlist_get(ns->routerstatus_list, i);
    }
    if (fast_memcmp(rs1->identity_digest, rs2->identity_digest, DIGEST_LEN)
        >= 0) {
      log_warn(LD_DIR, "Networkstatus entries not sorted by identity digest");
      goto err;
    }
  }
  if (ns_type != NS_TYPE_CONSENSUS) {
    digest256map_t *ed_id_map = digest256map_new();
    SMARTLIST_FOREACH_BEGIN(ns->routerstatus_list, vote_routerstatus_t *,
                            vrs) {
      if (! vrs->has_ed25519_listing ||
          tor_mem_is_zero((const char *)vrs->ed25519_id, DIGEST256_LEN))
        continue;
      if (digest256map_get(ed_id_map, vrs->ed25519_id) != NULL) {
        log_warn(LD_DIR, "Vote networkstatus ed25519 identities were not "
                 "unique");
        digest256map_free(ed_id_map, NULL);
        goto err;
      }
      digest256map_set(ed_id_map, vrs->ed25519_id, (void*)1);
    } SMARTLIST_FOREACH_END(vrs);
    digest256map_free(ed_id_map, NULL);
  }

  /* Parse footer; check signature. */
  footer_tokens = smartlist_new();
  if ((end_of_footer = strstr(s, "\nnetwork-status-version ")))
    ++end_of_footer;
  else
    end_of_footer = s + strlen(s);
  if (tokenize_string(area,s, end_of_footer, footer_tokens,
                      networkstatus_vote_footer_token_table, 0)) {
    log_warn(LD_DIR, "Error tokenizing network-status vote footer.");
    goto err;
  }

  {
    int found_sig = 0;
    SMARTLIST_FOREACH_BEGIN(footer_tokens, directory_token_t *, _tok) {
      tok = _tok;
      if (tok->tp == K_DIRECTORY_SIGNATURE)
        found_sig = 1;
      else if (found_sig) {
        log_warn(LD_DIR, "Extraneous token after first directory-signature");
        goto err;
      }
    } SMARTLIST_FOREACH_END(_tok);
  }

  if ((tok = find_opt_by_keyword(footer_tokens, K_DIRECTORY_FOOTER))) {
    if (tok != smartlist_get(footer_tokens, 0)) {
      log_warn(LD_DIR, "Misplaced directory-footer token");
      goto err;
    }
  }

  tok = find_opt_by_keyword(footer_tokens, K_BW_WEIGHTS);
  if (tok) {
    ns->weight_params = smartlist_new();
    for (i = 0; i < tok->n_args; ++i) {
      int ok=0;
      char *eq = strchr(tok->args[i], '=');
      if (!eq) {
        log_warn(LD_DIR, "Bad element '%s' in weight params",
                 escaped(tok->args[i]));
        goto err;
      }
      tor_parse_long(eq+1, 10, INT32_MIN, INT32_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_DIR, "Bad element '%s' in params", escaped(tok->args[i]));
        goto err;
      }
      smartlist_add(ns->weight_params, tor_strdup(tok->args[i]));
    }
  }

  SMARTLIST_FOREACH_BEGIN(footer_tokens, directory_token_t *, _tok) {
    char declared_identity[DIGEST_LEN];
    networkstatus_voter_info_t *v;
    document_signature_t *sig;
    const char *id_hexdigest = NULL;
    const char *sk_hexdigest = NULL;
    digest_algorithm_t alg = DIGEST_SHA1;
    tok = _tok;
    if (tok->tp != K_DIRECTORY_SIGNATURE)
      continue;
    tor_assert(tok->n_args >= 2);
    if (tok->n_args == 2) {
      id_hexdigest = tok->args[0];
      sk_hexdigest = tok->args[1];
    } else {
      const char *algname = tok->args[0];
      int a;
      id_hexdigest = tok->args[1];
      sk_hexdigest = tok->args[2];
      a = crypto_digest_algorithm_parse_name(algname);
      if (a<0) {
        log_warn(LD_DIR, "Unknown digest algorithm %s; skipping",
                 escaped(algname));
        continue;
      }
      alg = a;
    }

    if (!tok->object_type ||
        strcmp(tok->object_type, "SIGNATURE") ||
        tok->object_size < 128 || tok->object_size > 512) {
      log_warn(LD_DIR, "Bad object type or length on directory-signature");
      goto err;
    }

    if (strlen(id_hexdigest) != HEX_DIGEST_LEN ||
        base16_decode(declared_identity, sizeof(declared_identity),
                      id_hexdigest, HEX_DIGEST_LEN)
                      != sizeof(declared_identity)) {
      log_warn(LD_DIR, "Error decoding declared identity %s in "
               "network-status document.", escaped(id_hexdigest));
      goto err;
    }
    if (!(v = networkstatus_get_voter_by_id(ns, declared_identity))) {
      log_warn(LD_DIR, "ID on signature on network-status document does "
               "not match any declared directory source.");
      goto err;
    }
    sig = tor_malloc_zero(sizeof(document_signature_t));
    memcpy(sig->identity_digest, v->identity_digest, DIGEST_LEN);
    sig->alg = alg;
    if (strlen(sk_hexdigest) != HEX_DIGEST_LEN ||
        base16_decode(sig->signing_key_digest, sizeof(sig->signing_key_digest),
                      sk_hexdigest, HEX_DIGEST_LEN)
                      != sizeof(sig->signing_key_digest)) {
      log_warn(LD_DIR, "Error decoding declared signing key digest %s in "
               "network-status document.", escaped(sk_hexdigest));
      tor_free(sig);
      goto err;
    }

    if (ns->type != NS_TYPE_CONSENSUS) {
      if (tor_memneq(declared_identity, ns->cert->cache_info.identity_digest,
                 DIGEST_LEN)) {
        log_warn(LD_DIR, "Digest mismatch between declared and actual on "
                 "network-status vote.");
        tor_free(sig);
        goto err;
      }
    }

    if (voter_get_sig_by_algorithm(v, sig->alg)) {
      /* We already parsed a vote with this algorithm from this voter. Use the
         first one. */
      log_fn(LOG_PROTOCOL_WARN, LD_DIR, "We received a networkstatus "
             "that contains two signatures from the same voter with the same "
             "algorithm. Ignoring the second signature.");
      tor_free(sig);
      continue;
    }

    if (ns->type != NS_TYPE_CONSENSUS) {
      if (check_signature_token(ns_digests.d[DIGEST_SHA1], DIGEST_LEN,
                                tok, ns->cert->signing_key, 0,
                                "network-status document")) {
        tor_free(sig);
        goto err;
      }
      sig->good_signature = 1;
    } else {
      if (tok->object_size >= INT_MAX || tok->object_size >= SIZE_T_CEILING) {
        tor_free(sig);
        goto err;
      }
      sig->signature = tor_memdup(tok->object_body, tok->object_size);
      sig->signature_len = (int) tok->object_size;
    }
    smartlist_add(v->sigs, sig);

    ++n_signatures;
  } SMARTLIST_FOREACH_END(_tok);

  if (! n_signatures) {
    log_warn(LD_DIR, "No signatures on networkstatus document.");
    goto err;
  } else if (ns->type == NS_TYPE_VOTE && n_signatures != 1) {
    log_warn(LD_DIR, "Received more than one signature on a "
             "network-status vote.");
    goto err;
  }

  if (eos_out)
    *eos_out = end_of_footer;

  goto done;
 err:
  dump_desc(s_dup, "v3 networkstatus");
  networkstatus_vote_free(ns);
  ns = NULL;
 done:
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  if (voter) {
    if (voter->sigs) {
      SMARTLIST_FOREACH(voter->sigs, document_signature_t *, sig,
                        document_signature_free(sig));
      smartlist_free(voter->sigs);
    }
    tor_free(voter->nickname);
    tor_free(voter->address);
    tor_free(voter->contact);
    tor_free(voter);
  }
  if (rs_tokens) {
    SMARTLIST_FOREACH(rs_tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(rs_tokens);
  }
  if (footer_tokens) {
    SMARTLIST_FOREACH(footer_tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(footer_tokens);
  }
  if (area) {
    DUMP_AREA(area, "v3 networkstatus");
    memarea_drop_all(area);
  }
  if (rs_area)
    memarea_drop_all(rs_area);
  tor_free(last_kwd);

  return ns;
}

/** Return the common_digests_t that holds the digests of the
 * <b>flavor_name</b>-flavored networkstatus according to the detached
 * signatures document <b>sigs</b>, allocating a new common_digests_t as
 * neeeded. */
static common_digests_t *
detached_get_digests(ns_detached_signatures_t *sigs, const char *flavor_name)
{
  common_digests_t *d = strmap_get(sigs->digests, flavor_name);
  if (!d) {
    d = tor_malloc_zero(sizeof(common_digests_t));
    strmap_set(sigs->digests, flavor_name, d);
  }
  return d;
}

/** Return the list of signatures of the <b>flavor_name</b>-flavored
 * networkstatus according to the detached signatures document <b>sigs</b>,
 * allocating a new common_digests_t as neeeded. */
static smartlist_t *
detached_get_signatures(ns_detached_signatures_t *sigs,
                        const char *flavor_name)
{
  smartlist_t *sl = strmap_get(sigs->signatures, flavor_name);
  if (!sl) {
    sl = smartlist_new();
    strmap_set(sigs->signatures, flavor_name, sl);
  }
  return sl;
}

/** Parse a detached v3 networkstatus signature document between <b>s</b> and
 * <b>eos</b> and return the result.  Return -1 on failure. */
ns_detached_signatures_t *
networkstatus_parse_detached_signatures(const char *s, const char *eos)
{
  /* XXXX there is too much duplicate shared between this function and
   * networkstatus_parse_vote_from_string(). */
  directory_token_t *tok;
  memarea_t *area = NULL;
  common_digests_t *digests;

  smartlist_t *tokens = smartlist_new();
  ns_detached_signatures_t *sigs =
    tor_malloc_zero(sizeof(ns_detached_signatures_t));
  sigs->digests = strmap_new();
  sigs->signatures = strmap_new();

  if (!eos)
    eos = s + strlen(s);

  area = memarea_new();
  if (tokenize_string(area,s, eos, tokens,
                      networkstatus_detached_signature_token_table, 0)) {
    log_warn(LD_DIR, "Error tokenizing detached networkstatus signatures");
    goto err;
  }

  /* Grab all the digest-like tokens. */
  SMARTLIST_FOREACH_BEGIN(tokens, directory_token_t *, _tok) {
    const char *algname;
    digest_algorithm_t alg;
    const char *flavor;
    const char *hexdigest;
    size_t expected_length, digest_length;

    tok = _tok;

    if (tok->tp == K_CONSENSUS_DIGEST) {
      algname = "sha1";
      alg = DIGEST_SHA1;
      flavor = "ns";
      hexdigest = tok->args[0];
    } else if (tok->tp == K_ADDITIONAL_DIGEST) {
      int a = crypto_digest_algorithm_parse_name(tok->args[1]);
      if (a<0) {
        log_warn(LD_DIR, "Unrecognized algorithm name %s", tok->args[0]);
        continue;
      }
      alg = (digest_algorithm_t) a;
      flavor = tok->args[0];
      algname = tok->args[1];
      hexdigest = tok->args[2];
    } else {
      continue;
    }

    digest_length = crypto_digest_algorithm_get_length(alg);
    expected_length = digest_length * 2; /* hex encoding */

    if (strlen(hexdigest) != expected_length) {
      log_warn(LD_DIR, "Wrong length on consensus-digest in detached "
               "networkstatus signatures");
      goto err;
    }
    digests = detached_get_digests(sigs, flavor);
    tor_assert(digests);
    if (!tor_mem_is_zero(digests->d[alg], digest_length)) {
      log_warn(LD_DIR, "Multiple digests for %s with %s on detached "
               "signatures document", flavor, algname);
      continue;
    }
    if (base16_decode(digests->d[alg], digest_length,
                      hexdigest, strlen(hexdigest)) != (int) digest_length) {
      log_warn(LD_DIR, "Bad encoding on consensus-digest in detached "
               "networkstatus signatures");
      goto err;
    }
  } SMARTLIST_FOREACH_END(_tok);

  tok = find_by_keyword(tokens, K_VALID_AFTER);
  if (parse_iso_time(tok->args[0], &sigs->valid_after)) {
    log_warn(LD_DIR, "Bad valid-after in detached networkstatus signatures");
    goto err;
  }

  tok = find_by_keyword(tokens, K_FRESH_UNTIL);
  if (parse_iso_time(tok->args[0], &sigs->fresh_until)) {
    log_warn(LD_DIR, "Bad fresh-until in detached networkstatus signatures");
    goto err;
  }

  tok = find_by_keyword(tokens, K_VALID_UNTIL);
  if (parse_iso_time(tok->args[0], &sigs->valid_until)) {
    log_warn(LD_DIR, "Bad valid-until in detached networkstatus signatures");
    goto err;
  }

  SMARTLIST_FOREACH_BEGIN(tokens, directory_token_t *, _tok) {
    const char *id_hexdigest;
    const char *sk_hexdigest;
    const char *algname;
    const char *flavor;
    digest_algorithm_t alg;

    char id_digest[DIGEST_LEN];
    char sk_digest[DIGEST_LEN];
    smartlist_t *siglist;
    document_signature_t *sig;
    int is_duplicate;

    tok = _tok;
    if (tok->tp == K_DIRECTORY_SIGNATURE) {
      tor_assert(tok->n_args >= 2);
      flavor = "ns";
      algname = "sha1";
      id_hexdigest = tok->args[0];
      sk_hexdigest = tok->args[1];
    } else if (tok->tp == K_ADDITIONAL_SIGNATURE) {
      tor_assert(tok->n_args >= 4);
      flavor = tok->args[0];
      algname = tok->args[1];
      id_hexdigest = tok->args[2];
      sk_hexdigest = tok->args[3];
    } else {
      continue;
    }

    {
      int a = crypto_digest_algorithm_parse_name(algname);
      if (a<0) {
        log_warn(LD_DIR, "Unrecognized algorithm name %s", algname);
        continue;
      }
      alg = (digest_algorithm_t) a;
    }

    if (!tok->object_type ||
        strcmp(tok->object_type, "SIGNATURE") ||
        tok->object_size < 128 || tok->object_size > 512) {
      log_warn(LD_DIR, "Bad object type or length on directory-signature");
      goto err;
    }

    if (strlen(id_hexdigest) != HEX_DIGEST_LEN ||
        base16_decode(id_digest, sizeof(id_digest),
                      id_hexdigest, HEX_DIGEST_LEN) != sizeof(id_digest)) {
      log_warn(LD_DIR, "Error decoding declared identity %s in "
               "network-status vote.", escaped(id_hexdigest));
      goto err;
    }
    if (strlen(sk_hexdigest) != HEX_DIGEST_LEN ||
        base16_decode(sk_digest, sizeof(sk_digest),
                      sk_hexdigest, HEX_DIGEST_LEN) != sizeof(sk_digest)) {
      log_warn(LD_DIR, "Error decoding declared signing key digest %s in "
               "network-status vote.", escaped(sk_hexdigest));
      goto err;
    }

    siglist = detached_get_signatures(sigs, flavor);
    is_duplicate = 0;
    SMARTLIST_FOREACH(siglist, document_signature_t *, dsig, {
      if (dsig->alg == alg &&
          tor_memeq(id_digest, dsig->identity_digest, DIGEST_LEN) &&
          tor_memeq(sk_digest, dsig->signing_key_digest, DIGEST_LEN)) {
        is_duplicate = 1;
      }
    });
    if (is_duplicate) {
      log_warn(LD_DIR, "Two signatures with identical keys and algorithm "
               "found.");
      continue;
    }

    sig = tor_malloc_zero(sizeof(document_signature_t));
    sig->alg = alg;
    memcpy(sig->identity_digest, id_digest, DIGEST_LEN);
    memcpy(sig->signing_key_digest, sk_digest, DIGEST_LEN);
    if (tok->object_size >= INT_MAX || tok->object_size >= SIZE_T_CEILING) {
      tor_free(sig);
      goto err;
    }
    sig->signature = tor_memdup(tok->object_body, tok->object_size);
    sig->signature_len = (int) tok->object_size;

    smartlist_add(siglist, sig);
  } SMARTLIST_FOREACH_END(_tok);

  goto done;
 err:
  ns_detached_signatures_free(sigs);
  sigs = NULL;
 done:
  SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
  smartlist_free(tokens);
  if (area) {
    DUMP_AREA(area, "detached signatures");
    memarea_drop_all(area);
  }
  return sigs;
}

/** Parse the addr policy in the string <b>s</b> and return it.  If
 * assume_action is nonnegative, then insert its action (ADDR_POLICY_ACCEPT or
 * ADDR_POLICY_REJECT) for items that specify no action.
 *
 * Returns NULL on policy errors.
 *
 * Set *<b>malformed_list</b> to true if the entire policy list should be
 * discarded. Otherwise, set it to false, and only this item should be ignored
 * on error - the rest of the policy list can continue to be processed and
 * used.
 *
 * The addr_policy_t returned by this function can have its address set to
 * AF_UNSPEC for '*'.  Use policy_expand_unspec() to turn this into a pair
 * of AF_INET and AF_INET6 items.
 */
MOCK_IMPL(addr_policy_t *,
router_parse_addr_policy_item_from_string,(const char *s, int assume_action,
                                           int *malformed_list))
{
  directory_token_t *tok = NULL;
  const char *cp, *eos;
  /* Longest possible policy is
   * "accept6 [ffff:ffff:..255]/128:10000-65535",
   * which contains a max-length IPv6 address, plus 26 characters.
   * But note that there can be an arbitrary amount of space between the
   * accept and the address:mask/port element.
   * We don't need to multiply TOR_ADDR_BUF_LEN by 2, as there is only one
   * IPv6 address. But making the buffer shorter might cause valid long lines,
   * which parsed in previous versions, to fail to parse in new versions.
   * (These lines would have to have excessive amounts of whitespace.) */
  char line[TOR_ADDR_BUF_LEN*2 + 32];
  addr_policy_t *r;
  memarea_t *area = NULL;

  tor_assert(malformed_list);
  *malformed_list = 0;

  s = eat_whitespace(s);
  /* We can only do assume_action on []-quoted IPv6, as "a" (accept)
   * and ":" (port separator) are ambiguous */
  if ((*s == '*' || *s == '[' || TOR_ISDIGIT(*s)) && assume_action >= 0) {
    if (tor_snprintf(line, sizeof(line), "%s %s",
               assume_action == ADDR_POLICY_ACCEPT?"accept":"reject", s)<0) {
      log_warn(LD_DIR, "Policy %s is too long.", escaped(s));
      return NULL;
    }
    cp = line;
    tor_strlower(line);
  } else { /* assume an already well-formed address policy line */
    cp = s;
  }

  eos = cp + strlen(cp);
  area = memarea_new();
  tok = get_next_token(area, &cp, eos, routerdesc_token_table);
  if (tok->tp == ERR_) {
    log_warn(LD_DIR, "Error reading address policy: %s", tok->error);
    goto err;
  }
  if (tok->tp != K_ACCEPT && tok->tp != K_ACCEPT6 &&
      tok->tp != K_REJECT && tok->tp != K_REJECT6) {
    log_warn(LD_DIR, "Expected 'accept' or 'reject'.");
    goto err;
  }

  /* Use the extended interpretation of accept/reject *,
   * expanding it into an IPv4 wildcard and an IPv6 wildcard.
   * Also permit *4 and *6 for IPv4 and IPv6 only wildcards. */
  r = router_parse_addr_policy(tok, TAPMP_EXTENDED_STAR);
  if (!r) {
    goto err;
  }

  /* Ensure that accept6/reject6 fields are followed by IPv6 addresses.
   * AF_UNSPEC addresses are only permitted on the accept/reject field type.
   * Unlike descriptors, torrcs exit policy accept/reject can be followed by
   * either an IPv4 or IPv6 address. */
  if ((tok->tp == K_ACCEPT6 || tok->tp == K_REJECT6) &&
       tor_addr_family(&r->addr) != AF_INET6) {
    /* This is a non-fatal error, just ignore this one entry. */
    *malformed_list = 0;
    log_warn(LD_DIR, "IPv4 address '%s' with accept6/reject6 field type in "
             "exit policy. Ignoring, but continuing to parse rules. (Use "
             "accept/reject with IPv4 addresses.)",
             tok->n_args == 1 ? tok->args[0] : "");
    addr_policy_free(r);
    r = NULL;
    goto done;
  }

  goto done;
 err:
  *malformed_list = 1;
  r = NULL;
 done:
  token_clear(tok);
  if (area) {
    DUMP_AREA(area, "policy item");
    memarea_drop_all(area);
  }
  return r;
}

/** Add an exit policy stored in the token <b>tok</b> to the router info in
 * <b>router</b>.  Return 0 on success, -1 on failure. */
static int
router_add_exit_policy(routerinfo_t *router, directory_token_t *tok)
{
  addr_policy_t *newe;
  /* Use the standard interpretation of accept/reject *, an IPv4 wildcard. */
  newe = router_parse_addr_policy(tok, 0);
  if (!newe)
    return -1;
  if (! router->exit_policy)
    router->exit_policy = smartlist_new();

  /* Ensure that in descriptors, accept/reject fields are followed by
   * IPv4 addresses, and accept6/reject6 fields are followed by
   * IPv6 addresses. Unlike torrcs, descriptor exit policies do not permit
   * accept/reject followed by IPv6. */
  if (((tok->tp == K_ACCEPT6 || tok->tp == K_REJECT6) &&
       tor_addr_family(&newe->addr) == AF_INET)
      ||
      ((tok->tp == K_ACCEPT || tok->tp == K_REJECT) &&
       tor_addr_family(&newe->addr) == AF_INET6)) {
    /* There's nothing the user can do about other relays' descriptors,
     * so we don't provide usage advice here. */
    log_warn(LD_DIR, "Mismatch between field type and address type in exit "
             "policy '%s'. Discarding entire router descriptor.",
             tok->n_args == 1 ? tok->args[0] : "");
    addr_policy_free(newe);
    return -1;
  }

  smartlist_add(router->exit_policy, newe);

  return 0;
}

/** Given a K_ACCEPT[6] or K_REJECT[6] token and a router, create and return
 * a new exit_policy_t corresponding to the token. If TAPMP_EXTENDED_STAR
 * is set in fmt_flags, K_ACCEPT6 and K_REJECT6 tokens followed by *
 * expand to IPv6-only policies, otherwise they expand to IPv4 and IPv6
 * policies */
static addr_policy_t *
router_parse_addr_policy(directory_token_t *tok, unsigned fmt_flags)
{
  addr_policy_t newe;
  char *arg;

  tor_assert(tok->tp == K_REJECT || tok->tp == K_REJECT6 ||
             tok->tp == K_ACCEPT || tok->tp == K_ACCEPT6);

  if (tok->n_args != 1)
    return NULL;
  arg = tok->args[0];

  if (!strcmpstart(arg,"private"))
    return router_parse_addr_policy_private(tok);

  memset(&newe, 0, sizeof(newe));

  if (tok->tp == K_REJECT || tok->tp == K_REJECT6)
    newe.policy_type = ADDR_POLICY_REJECT;
  else
    newe.policy_type = ADDR_POLICY_ACCEPT;

  /* accept6/reject6 * produces an IPv6 wildcard address only.
   * (accept/reject * produces rules for IPv4 and IPv6 wildcard addresses.) */
  if ((fmt_flags & TAPMP_EXTENDED_STAR)
      && (tok->tp == K_ACCEPT6 || tok->tp == K_REJECT6)) {
    fmt_flags |= TAPMP_STAR_IPV6_ONLY;
  }

  if (tor_addr_parse_mask_ports(arg, fmt_flags, &newe.addr, &newe.maskbits,
                                &newe.prt_min, &newe.prt_max) < 0) {
    log_warn(LD_DIR,"Couldn't parse line %s. Dropping", escaped(arg));
    return NULL;
  }

  return addr_policy_get_canonical_entry(&newe);
}

/** Parse an exit policy line of the format "accept[6]/reject[6] private:...".
 * This didn't exist until Tor 0.1.1.15, so nobody should generate it in
 * router descriptors until earlier versions are obsolete.
 *
 * accept/reject and accept6/reject6 private all produce rules for both
 * IPv4 and IPv6 addresses.
 */
static addr_policy_t *
router_parse_addr_policy_private(directory_token_t *tok)
{
  const char *arg;
  uint16_t port_min, port_max;
  addr_policy_t result;

  arg = tok->args[0];
  if (strcmpstart(arg, "private"))
    return NULL;

  arg += strlen("private");
  arg = (char*) eat_whitespace(arg);
  if (!arg || *arg != ':')
    return NULL;

  if (parse_port_range(arg+1, &port_min, &port_max)<0)
    return NULL;

  memset(&result, 0, sizeof(result));
  if (tok->tp == K_REJECT || tok->tp == K_REJECT6)
    result.policy_type = ADDR_POLICY_REJECT;
  else
    result.policy_type = ADDR_POLICY_ACCEPT;
  result.is_private = 1;
  result.prt_min = port_min;
  result.prt_max = port_max;

  if (tok->tp == K_ACCEPT6 || tok->tp == K_REJECT6) {
    log_warn(LD_GENERAL,
             "'%s' expands into rules which apply to all private IPv4 and "
             "IPv6 addresses. (Use accept/reject private:* for IPv4 and "
             "IPv6.)", tok->n_args == 1 ? tok->args[0] : "");
  }

  return addr_policy_get_canonical_entry(&result);
}

/** Log and exit if <b>t</b> is malformed */
void
assert_addr_policy_ok(smartlist_t *lst)
{
  if (!lst) return;
  SMARTLIST_FOREACH(lst, addr_policy_t *, t, {
    tor_assert(t->policy_type == ADDR_POLICY_REJECT ||
               t->policy_type == ADDR_POLICY_ACCEPT);
    tor_assert(t->prt_min <= t->prt_max);
  });
}

/*
 * Low-level tokenizer for router descriptors and directories.
 */

/** Free all resources allocated for <b>tok</b> */
static void
token_clear(directory_token_t *tok)
{
  if (tok->key)
    crypto_pk_free(tok->key);
}

#define ALLOC_ZERO(sz) memarea_alloc_zero(area,sz)
#define ALLOC(sz) memarea_alloc(area,sz)
#define STRDUP(str) memarea_strdup(area,str)
#define STRNDUP(str,n) memarea_strndup(area,(str),(n))

#define RET_ERR(msg)                                               \
  STMT_BEGIN                                                       \
    if (tok) token_clear(tok);                                      \
    tok = ALLOC_ZERO(sizeof(directory_token_t));                   \
    tok->tp = ERR_;                                                \
    tok->error = STRDUP(msg);                                      \
    goto done_tokenizing;                                          \
  STMT_END

/** Helper: make sure that the token <b>tok</b> with keyword <b>kwd</b> obeys
 * the object syntax of <b>o_syn</b>.  Allocate all storage in <b>area</b>.
 * Return <b>tok</b> on success, or a new ERR_ token if the token didn't
 * conform to the syntax we wanted.
 **/
static inline directory_token_t *
token_check_object(memarea_t *area, const char *kwd,
                   directory_token_t *tok, obj_syntax o_syn)
{
  char ebuf[128];
  switch (o_syn) {
    case NO_OBJ:
      /* No object is allowed for this token. */
      if (tok->object_body) {
        tor_snprintf(ebuf, sizeof(ebuf), "Unexpected object for %s", kwd);
        RET_ERR(ebuf);
      }
      if (tok->key) {
        tor_snprintf(ebuf, sizeof(ebuf), "Unexpected public key for %s", kwd);
        RET_ERR(ebuf);
      }
      break;
    case NEED_OBJ:
      /* There must be a (non-key) object. */
      if (!tok->object_body) {
        tor_snprintf(ebuf, sizeof(ebuf), "Missing object for %s", kwd);
        RET_ERR(ebuf);
      }
      break;
    case NEED_KEY_1024: /* There must be a 1024-bit public key. */
    case NEED_SKEY_1024: /* There must be a 1024-bit private key. */
      if (tok->key && crypto_pk_num_bits(tok->key) != PK_BYTES*8) {
        tor_snprintf(ebuf, sizeof(ebuf), "Wrong size on key for %s: %d bits",
                     kwd, crypto_pk_num_bits(tok->key));
        RET_ERR(ebuf);
      }
      /* fall through */
    case NEED_KEY: /* There must be some kind of key. */
      if (!tok->key) {
        tor_snprintf(ebuf, sizeof(ebuf), "Missing public key for %s", kwd);
        RET_ERR(ebuf);
      }
      if (o_syn != NEED_SKEY_1024) {
        if (crypto_pk_key_is_private(tok->key)) {
          tor_snprintf(ebuf, sizeof(ebuf),
               "Private key given for %s, which wants a public key", kwd);
          RET_ERR(ebuf);
        }
      } else { /* o_syn == NEED_SKEY_1024 */
        if (!crypto_pk_key_is_private(tok->key)) {
          tor_snprintf(ebuf, sizeof(ebuf),
               "Public key given for %s, which wants a private key", kwd);
          RET_ERR(ebuf);
        }
      }
      break;
    case OBJ_OK:
      /* Anything goes with this token. */
      break;
  }

 done_tokenizing:
  return tok;
}

/** Helper: parse space-separated arguments from the string <b>s</b> ending at
 * <b>eol</b>, and store them in the args field of <b>tok</b>.  Store the
 * number of parsed elements into the n_args field of <b>tok</b>.  Allocate
 * all storage in <b>area</b>.  Return the number of arguments parsed, or
 * return -1 if there was an insanely high number of arguments. */
static inline int
get_token_arguments(memarea_t *area, directory_token_t *tok,
                    const char *s, const char *eol)
{
/** Largest number of arguments we'll accept to any token, ever. */
#define MAX_ARGS 512
  char *mem = memarea_strndup(area, s, eol-s);
  char *cp = mem;
  int j = 0;
  char *args[MAX_ARGS];
  while (*cp) {
    if (j == MAX_ARGS)
      return -1;
    args[j++] = cp;
    cp = (char*)find_whitespace(cp);
    if (!cp || !*cp)
      break; /* End of the line. */
    *cp++ = '\0';
    cp = (char*)eat_whitespace(cp);
  }
  tok->n_args = j;
  tok->args = memarea_memdup(area, args, j*sizeof(char*));
  return j;
#undef MAX_ARGS
}

/** Helper function: read the next token from *s, advance *s to the end of the
 * token, and return the parsed token.  Parse *<b>s</b> according to the list
 * of tokens in <b>table</b>.
 */
static directory_token_t *
get_next_token(memarea_t *area,
               const char **s, const char *eos, token_rule_t *table)
{
  /** Reject any object at least this big; it is probably an overflow, an
   * attack, a bug, or some other nonsense. */
#define MAX_UNPARSED_OBJECT_SIZE (128*1024)
  /** Reject any line at least this big; it is probably an overflow, an
   * attack, a bug, or some other nonsense. */
#define MAX_LINE_LENGTH (128*1024)

  const char *next, *eol, *obstart;
  size_t obname_len;
  int i;
  directory_token_t *tok;
  obj_syntax o_syn = NO_OBJ;
  char ebuf[128];
  const char *kwd = "";

  tor_assert(area);
  tok = ALLOC_ZERO(sizeof(directory_token_t));
  tok->tp = ERR_;

  /* Set *s to first token, eol to end-of-line, next to after first token */
  *s = eat_whitespace_eos(*s, eos); /* eat multi-line whitespace */
  tor_assert(eos >= *s);
  eol = memchr(*s, '\n', eos-*s);
  if (!eol)
    eol = eos;
  if (eol - *s > MAX_LINE_LENGTH) {
    RET_ERR("Line far too long");
  }

  next = find_whitespace_eos(*s, eol);

  if (!strcmp_len(*s, "opt", next-*s)) {
    /* Skip past an "opt" at the start of the line. */
    *s = eat_whitespace_eos_no_nl(next, eol);
    next = find_whitespace_eos(*s, eol);
  } else if (*s == eos) {  /* If no "opt", and end-of-line, line is invalid */
    RET_ERR("Unexpected EOF");
  }

  /* Search the table for the appropriate entry.  (I tried a binary search
   * instead, but it wasn't any faster.) */
  for (i = 0; table[i].t ; ++i) {
    if (!strcmp_len(*s, table[i].t, next-*s)) {
      /* We've found the keyword. */
      kwd = table[i].t;
      tok->tp = table[i].v;
      o_syn = table[i].os;
      *s = eat_whitespace_eos_no_nl(next, eol);
      /* We go ahead whether there are arguments or not, so that tok->args is
       * always set if we want arguments. */
      if (table[i].concat_args) {
        /* The keyword takes the line as a single argument */
        tok->args = ALLOC(sizeof(char*));
        tok->args[0] = STRNDUP(*s,eol-*s); /* Grab everything on line */
        tok->n_args = 1;
      } else {
        /* This keyword takes multiple arguments. */
        if (get_token_arguments(area, tok, *s, eol)<0) {
          tor_snprintf(ebuf, sizeof(ebuf),"Far too many arguments to %s", kwd);
          RET_ERR(ebuf);
        }
        *s = eol;
      }
      if (tok->n_args < table[i].min_args) {
        tor_snprintf(ebuf, sizeof(ebuf), "Too few arguments to %s", kwd);
        RET_ERR(ebuf);
      } else if (tok->n_args > table[i].max_args) {
        tor_snprintf(ebuf, sizeof(ebuf), "Too many arguments to %s", kwd);
        RET_ERR(ebuf);
      }
      break;
    }
  }

  if (tok->tp == ERR_) {
    /* No keyword matched; call it an "K_opt" or "A_unrecognized" */
    if (*s < eol && **s == '@')
      tok->tp = A_UNKNOWN_;
    else
      tok->tp = K_OPT;
    tok->args = ALLOC(sizeof(char*));
    tok->args[0] = STRNDUP(*s, eol-*s);
    tok->n_args = 1;
    o_syn = OBJ_OK;
  }

  /* Check whether there's an object present */
  *s = eat_whitespace_eos(eol, eos);  /* Scan from end of first line */
  tor_assert(eos >= *s);
  eol = memchr(*s, '\n', eos-*s);
  if (!eol || eol-*s<11 || strcmpstart(*s, "-----BEGIN ")) /* No object. */
    goto check_object;

  obstart = *s; /* Set obstart to start of object spec */
  if (*s+16 >= eol || memchr(*s+11,'\0',eol-*s-16) || /* no short lines, */
      strcmp_len(eol-5, "-----", 5) ||           /* nuls or invalid endings */
      (eol-*s) > MAX_UNPARSED_OBJECT_SIZE) {     /* name too long */
    RET_ERR("Malformed object: bad begin line");
  }
  tok->object_type = STRNDUP(*s+11, eol-*s-16);
  obname_len = eol-*s-16; /* store objname length here to avoid a strlen() */
  *s = eol+1;    /* Set *s to possible start of object data (could be eos) */

  /* Go to the end of the object */
  next = tor_memstr(*s, eos-*s, "-----END ");
  if (!next) {
    RET_ERR("Malformed object: missing object end line");
  }
  tor_assert(eos >= next);
  eol = memchr(next, '\n', eos-next);
  if (!eol)  /* end-of-line marker, or eos if there's no '\n' */
    eol = eos;
  /* Validate the ending tag, which should be 9 + NAME + 5 + eol */
  if ((size_t)(eol-next) != 9+obname_len+5 ||
      strcmp_len(next+9, tok->object_type, obname_len) ||
      strcmp_len(eol-5, "-----", 5)) {
    tor_snprintf(ebuf, sizeof(ebuf), "Malformed object: mismatched end tag %s",
             tok->object_type);
    ebuf[sizeof(ebuf)-1] = '\0';
    RET_ERR(ebuf);
  }
  if (next - *s > MAX_UNPARSED_OBJECT_SIZE)
    RET_ERR("Couldn't parse object: missing footer or object much too big.");

  if (!strcmp(tok->object_type, "RSA PUBLIC KEY")) { /* If it's a public key */
    tok->key = crypto_pk_new();
    if (crypto_pk_read_public_key_from_string(tok->key, obstart, eol-obstart))
      RET_ERR("Couldn't parse public key.");
  } else if (!strcmp(tok->object_type, "RSA PRIVATE KEY")) { /* private key */
    tok->key = crypto_pk_new();
    if (crypto_pk_read_private_key_from_string(tok->key, obstart, eol-obstart))
      RET_ERR("Couldn't parse private key.");
  } else { /* If it's something else, try to base64-decode it */
    int r;
    tok->object_body = ALLOC(next-*s); /* really, this is too much RAM. */
    r = base64_decode(tok->object_body, next-*s, *s, next-*s);
    if (r<0)
      RET_ERR("Malformed object: bad base64-encoded data");
    tok->object_size = r;
  }
  *s = eol;

 check_object:
  tok = token_check_object(area, kwd, tok, o_syn);

 done_tokenizing:
  return tok;

#undef RET_ERR
#undef ALLOC
#undef ALLOC_ZERO
#undef STRDUP
#undef STRNDUP
}

/** Read all tokens from a string between <b>start</b> and <b>end</b>, and add
 * them to <b>out</b>.  Parse according to the token rules in <b>table</b>.
 * Caller must free tokens in <b>out</b>.  If <b>end</b> is NULL, use the
 * entire string.
 */
static int
tokenize_string(memarea_t *area,
                const char *start, const char *end, smartlist_t *out,
                token_rule_t *table, int flags)
{
  const char **s;
  directory_token_t *tok = NULL;
  int counts[NIL_];
  int i;
  int first_nonannotation;
  int prev_len = smartlist_len(out);
  tor_assert(area);

  s = &start;
  if (!end) {
    end = start+strlen(start);
  } else {
    /* it's only meaningful to check for nuls if we got an end-of-string ptr */
    if (memchr(start, '\0', end-start)) {
      log_warn(LD_DIR, "parse error: internal NUL character.");
      return -1;
    }
  }
  for (i = 0; i < NIL_; ++i)
    counts[i] = 0;

  SMARTLIST_FOREACH(out, const directory_token_t *, t, ++counts[t->tp]);

  while (*s < end && (!tok || tok->tp != EOF_)) {
    tok = get_next_token(area, s, end, table);
    if (tok->tp == ERR_) {
      log_warn(LD_DIR, "parse error: %s", tok->error);
      token_clear(tok);
      return -1;
    }
    ++counts[tok->tp];
    smartlist_add(out, tok);
    *s = eat_whitespace_eos(*s, end);
  }

  if (flags & TS_NOCHECK)
    return 0;

  if ((flags & TS_ANNOTATIONS_OK)) {
    first_nonannotation = -1;
    for (i = 0; i < smartlist_len(out); ++i) {
      tok = smartlist_get(out, i);
      if (tok->tp < MIN_ANNOTATION || tok->tp > MAX_ANNOTATION) {
        first_nonannotation = i;
        break;
      }
    }
    if (first_nonannotation < 0) {
      log_warn(LD_DIR, "parse error: item contains only annotations");
      return -1;
    }
    for (i=first_nonannotation;  i < smartlist_len(out); ++i) {
      tok = smartlist_get(out, i);
      if (tok->tp >= MIN_ANNOTATION && tok->tp <= MAX_ANNOTATION) {
        log_warn(LD_DIR, "parse error: Annotations mixed with keywords");
        return -1;
      }
    }
    if ((flags & TS_NO_NEW_ANNOTATIONS)) {
      if (first_nonannotation != prev_len) {
        log_warn(LD_DIR, "parse error: Unexpected annotations.");
        return -1;
      }
    }
  } else {
    for (i=0;  i < smartlist_len(out); ++i) {
      tok = smartlist_get(out, i);
      if (tok->tp >= MIN_ANNOTATION && tok->tp <= MAX_ANNOTATION) {
        log_warn(LD_DIR, "parse error: no annotations allowed.");
        return -1;
      }
    }
    first_nonannotation = 0;
  }
  for (i = 0; table[i].t; ++i) {
    if (counts[table[i].v] < table[i].min_cnt) {
      log_warn(LD_DIR, "Parse error: missing %s element.", table[i].t);
      return -1;
    }
    if (counts[table[i].v] > table[i].max_cnt) {
      log_warn(LD_DIR, "Parse error: too many %s elements.", table[i].t);
      return -1;
    }
    if (table[i].pos & AT_START) {
      if (smartlist_len(out) < 1 ||
          (tok = smartlist_get(out, first_nonannotation))->tp != table[i].v) {
        log_warn(LD_DIR, "Parse error: first item is not %s.", table[i].t);
        return -1;
      }
    }
    if (table[i].pos & AT_END) {
      if (smartlist_len(out) < 1 ||
          (tok = smartlist_get(out, smartlist_len(out)-1))->tp != table[i].v) {
        log_warn(LD_DIR, "Parse error: last item is not %s.", table[i].t);
        return -1;
      }
    }
  }
  return 0;
}

/** Find the first token in <b>s</b> whose keyword is <b>keyword</b>; return
 * NULL if no such keyword is found.
 */
static directory_token_t *
find_opt_by_keyword(smartlist_t *s, directory_keyword keyword)
{
  SMARTLIST_FOREACH(s, directory_token_t *, t, if (t->tp == keyword) return t);
  return NULL;
}

/** Find the first token in <b>s</b> whose keyword is <b>keyword</b>; fail
 * with an assert if no such keyword is found.
 */
static directory_token_t *
find_by_keyword_(smartlist_t *s, directory_keyword keyword,
                 const char *keyword_as_string)
{
  directory_token_t *tok = find_opt_by_keyword(s, keyword);
  if (PREDICT_UNLIKELY(!tok)) {
    log_err(LD_BUG, "Missing %s [%d] in directory object that should have "
         "been validated. Internal error.", keyword_as_string, (int)keyword);
    tor_assert(tok);
  }
  return tok;
}

/** If there are any directory_token_t entries in <b>s</b> whose keyword is
 * <b>k</b>, return a newly allocated smartlist_t containing all such entries,
 * in the same order in which they occur in <b>s</b>.  Otherwise return
 * NULL. */
static smartlist_t *
find_all_by_keyword(smartlist_t *s, directory_keyword k)
{
  smartlist_t *out = NULL;
  SMARTLIST_FOREACH(s, directory_token_t *, t,
                    if (t->tp == k) {
                      if (!out)
                        out = smartlist_new();
                      smartlist_add(out, t);
                    });
  return out;
}

/** Return a newly allocated smartlist of all accept or reject tokens in
 * <b>s</b>.
 */
static smartlist_t *
find_all_exitpolicy(smartlist_t *s)
{
  smartlist_t *out = smartlist_new();
  SMARTLIST_FOREACH(s, directory_token_t *, t,
      if (t->tp == K_ACCEPT || t->tp == K_ACCEPT6 ||
          t->tp == K_REJECT || t->tp == K_REJECT6)
        smartlist_add(out,t));
  return out;
}

/** Helper function for <b>router_get_hash_impl</b>: given <b>s</b>,
 * <b>s_len</b>, <b>start_str</b>, <b>end_str</b>, and <b>end_c</b> with the
 * same semantics as in that function, set *<b>start_out</b> (inclusive) and
 * *<b>end_out</b> (exclusive) to the boundaries of the string to be hashed.
 *
 * Return 0 on success and -1 on failure.
 */
static int
router_get_hash_impl_helper(const char *s, size_t s_len,
                            const char *start_str,
                            const char *end_str, char end_c,
                            const char **start_out, const char **end_out)
{
  const char *start, *end;
  start = tor_memstr(s, s_len, start_str);
  if (!start) {
    log_warn(LD_DIR,"couldn't find start of hashed material \"%s\"",start_str);
    return -1;
  }
  if (start != s && *(start-1) != '\n') {
    log_warn(LD_DIR,
             "first occurrence of \"%s\" is not at the start of a line",
             start_str);
    return -1;
  }
  end = tor_memstr(start+strlen(start_str),
                   s_len - (start-s) - strlen(start_str), end_str);
  if (!end) {
    log_warn(LD_DIR,"couldn't find end of hashed material \"%s\"",end_str);
    return -1;
  }
  end = memchr(end+strlen(end_str), end_c, s_len - (end-s) - strlen(end_str));
  if (!end) {
    log_warn(LD_DIR,"couldn't find EOL");
    return -1;
  }
  ++end;

  *start_out = start;
  *end_out = end;
  return 0;
}

/** Compute the digest of the substring of <b>s</b> taken from the first
 * occurrence of <b>start_str</b> through the first instance of c after the
 * first subsequent occurrence of <b>end_str</b>; store the 20-byte or 32-byte
 * result in <b>digest</b>; return 0 on success.
 *
 * If no such substring exists, return -1.
 */
static int
router_get_hash_impl(const char *s, size_t s_len, char *digest,
                     const char *start_str,
                     const char *end_str, char end_c,
                     digest_algorithm_t alg)
{
  const char *start=NULL, *end=NULL;
  if (router_get_hash_impl_helper(s,s_len,start_str,end_str,end_c,
                                  &start,&end)<0)
    return -1;

  if (alg == DIGEST_SHA1) {
    if (crypto_digest(digest, start, end-start)) {
      log_warn(LD_BUG,"couldn't compute digest");
      return -1;
    }
  } else {
    if (crypto_digest256(digest, start, end-start, alg)) {
      log_warn(LD_BUG,"couldn't compute digest");
      return -1;
    }
  }

  return 0;
}

/** As router_get_hash_impl, but compute all hashes. */
static int
router_get_hashes_impl(const char *s, size_t s_len, common_digests_t *digests,
                       const char *start_str,
                       const char *end_str, char end_c)
{
  const char *start=NULL, *end=NULL;
  if (router_get_hash_impl_helper(s,s_len,start_str,end_str,end_c,
                                  &start,&end)<0)
    return -1;

  if (crypto_common_digests(digests, start, end-start)) {
    log_warn(LD_BUG,"couldn't compute digests");
    return -1;
  }

  return 0;
}

/** Assuming that s starts with a microdesc, return the start of the
 * *NEXT* one.  Return NULL on "not found." */
static const char *
find_start_of_next_microdesc(const char *s, const char *eos)
{
  int started_with_annotations;
  s = eat_whitespace_eos(s, eos);
  if (!s)
    return NULL;

#define CHECK_LENGTH() STMT_BEGIN \
    if (s+32 > eos)               \
      return NULL;                \
  STMT_END

#define NEXT_LINE() STMT_BEGIN            \
    s = memchr(s, '\n', eos-s);           \
    if (!s || s+1 >= eos)                 \
      return NULL;                        \
    s++;                                  \
  STMT_END

  CHECK_LENGTH();

  started_with_annotations = (*s == '@');

  if (started_with_annotations) {
    /* Start by advancing to the first non-annotation line. */
    while (*s == '@')
      NEXT_LINE();
  }
  CHECK_LENGTH();

  /* Now we should be pointed at an onion-key line.  If we are, then skip
   * it. */
  if (!strcmpstart(s, "onion-key"))
    NEXT_LINE();

  /* Okay, now we're pointed at the first line of the microdescriptor which is
     not an annotation or onion-key.  The next line that _is_ an annotation or
     onion-key is the start of the next microdescriptor. */
  while (s+32 < eos) {
    if (*s == '@' || !strcmpstart(s, "onion-key"))
      return s;
    NEXT_LINE();
  }
  return NULL;

#undef CHECK_LENGTH
#undef NEXT_LINE
}

/** Parse as many microdescriptors as are found from the string starting at
 * <b>s</b> and ending at <b>eos</b>.  If allow_annotations is set, read any
 * annotations we recognize and ignore ones we don't.
 *
 * If <b>saved_location</b> isn't SAVED_IN_CACHE, make a local copy of each
 * descriptor in the body field of each microdesc_t.
 *
 * Return all newly parsed microdescriptors in a newly allocated
 * smartlist_t. If <b>invalid_disgests_out</b> is provided, add a SHA256
 * microdesc digest to it for every microdesc that we found to be badly
 * formed. (This may cause duplicates) */
smartlist_t *
microdescs_parse_from_string(const char *s, const char *eos,
                             int allow_annotations,
                             saved_location_t where,
                             smartlist_t *invalid_digests_out)
{
  smartlist_t *tokens;
  smartlist_t *result;
  microdesc_t *md = NULL;
  memarea_t *area;
  const char *start = s;
  const char *start_of_next_microdesc;
  int flags = allow_annotations ? TS_ANNOTATIONS_OK : 0;
  const int copy_body = (where != SAVED_IN_CACHE);

  directory_token_t *tok;

  if (!eos)
    eos = s + strlen(s);

  s = eat_whitespace_eos(s, eos);
  area = memarea_new();
  result = smartlist_new();
  tokens = smartlist_new();

  while (s < eos) {
    int okay = 0;

    start_of_next_microdesc = find_start_of_next_microdesc(s, eos);
    if (!start_of_next_microdesc)
      start_of_next_microdesc = eos;

    md = tor_malloc_zero(sizeof(microdesc_t));
    {
      const char *cp = tor_memstr(s, start_of_next_microdesc-s,
                                  "onion-key");
      const int no_onion_key = (cp == NULL);
      if (no_onion_key) {
        cp = s; /* So that we have *some* junk to put in the body */
      }

      md->bodylen = start_of_next_microdesc - cp;
      md->saved_location = where;
      if (copy_body)
        md->body = tor_memdup_nulterm(cp, md->bodylen);
      else
        md->body = (char*)cp;
      md->off = cp - start;
      crypto_digest256(md->digest, md->body, md->bodylen, DIGEST_SHA256);
      if (no_onion_key) {
        log_fn(LOG_PROTOCOL_WARN, LD_DIR, "Malformed or truncated descriptor");
        goto next;
      }
    }

    if (tokenize_string(area, s, start_of_next_microdesc, tokens,
                        microdesc_token_table, flags)) {
      log_warn(LD_DIR, "Unparseable microdescriptor");
      goto next;
    }

    if ((tok = find_opt_by_keyword(tokens, A_LAST_LISTED))) {
      if (parse_iso_time(tok->args[0], &md->last_listed)) {
        log_warn(LD_DIR, "Bad last-listed time in microdescriptor");
        goto next;
      }
    }

    tok = find_by_keyword(tokens, K_ONION_KEY);
    if (!crypto_pk_public_exponent_ok(tok->key)) {
      log_warn(LD_DIR,
               "Relay's onion key had invalid exponent.");
      goto next;
    }
    md->onion_pkey = tok->key;
    tok->key = NULL;

    if ((tok = find_opt_by_keyword(tokens, K_ONION_KEY_NTOR))) {
      curve25519_public_key_t k;
      tor_assert(tok->n_args >= 1);
      if (curve25519_public_from_base64(&k, tok->args[0]) < 0) {
        log_warn(LD_DIR, "Bogus ntor-onion-key in microdesc");
        goto next;
      }
      md->onion_curve25519_pkey =
        tor_memdup(&k, sizeof(curve25519_public_key_t));
    }

    smartlist_t *id_lines = find_all_by_keyword(tokens, K_ID);
    if (id_lines) {
      SMARTLIST_FOREACH_BEGIN(id_lines, directory_token_t *, t) {
        tor_assert(t->n_args >= 2);
        if (!strcmp(t->args[0], "ed25519")) {
          if (md->ed25519_identity_pkey) {
            log_warn(LD_DIR, "Extra ed25519 key in microdesc");
            goto next;
          }
          ed25519_public_key_t k;
          if (ed25519_public_from_base64(&k, t->args[1])<0) {
            log_warn(LD_DIR, "Bogus ed25519 key in microdesc");
            goto next;
          }
          md->ed25519_identity_pkey = tor_memdup(&k, sizeof(k));
        }
      } SMARTLIST_FOREACH_END(t);
      smartlist_free(id_lines);
    }

    {
      smartlist_t *a_lines = find_all_by_keyword(tokens, K_A);
      if (a_lines) {
        find_single_ipv6_orport(a_lines, &md->ipv6_addr, &md->ipv6_orport);
        smartlist_free(a_lines);
      }
    }

    if ((tok = find_opt_by_keyword(tokens, K_FAMILY))) {
      int i;
      md->family = smartlist_new();
      for (i=0;i<tok->n_args;++i) {
        if (!is_legal_nickname_or_hexdigest(tok->args[i])) {
          log_warn(LD_DIR, "Illegal nickname %s in family line",
                   escaped(tok->args[i]));
          goto next;
        }
        smartlist_add(md->family, tor_strdup(tok->args[i]));
      }
    }

    if ((tok = find_opt_by_keyword(tokens, K_P))) {
      md->exit_policy = parse_short_policy(tok->args[0]);
    }
    if ((tok = find_opt_by_keyword(tokens, K_P6))) {
      md->ipv6_exit_policy = parse_short_policy(tok->args[0]);
    }

    smartlist_add(result, md);
    okay = 1;

    md = NULL;
  next:
    if (! okay && invalid_digests_out) {
      smartlist_add(invalid_digests_out,
                    tor_memdup(md->digest, DIGEST256_LEN));
    }
    microdesc_free(md);
    md = NULL;

    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    memarea_clear(area);
    smartlist_clear(tokens);
    s = start_of_next_microdesc;
  }

  SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
  memarea_drop_all(area);
  smartlist_free(tokens);

  return result;
}

/** Extract a Tor version from a <b>platform</b> line from a router
 * descriptor, and place the result in <b>router_version</b>.
 *
 * Return 1 on success, -1 on parsing failure, and 0 if the
 * platform line does not indicate some version of Tor.
 *
 * If <b>strict</b> is non-zero, finding any weird version components
 * (like negative numbers) counts as a parsing failure.
 */
int
tor_version_parse_platform(const char *platform,
                           tor_version_t *router_version,
                           int strict)
{
  char tmp[128];
  char *s, *s2, *start;

  if (strcmpstart(platform,"Tor ")) /* nonstandard Tor; say 0. */
    return 0;

  start = (char *)eat_whitespace(platform+3);
  if (!*start) return -1;
  s = (char *)find_whitespace(start); /* also finds '\0', which is fine */
  s2 = (char*)eat_whitespace(s);
  if (!strcmpstart(s2, "(r") || !strcmpstart(s2, "(git-"))
    s = (char*)find_whitespace(s2);

  if ((size_t)(s-start+1) >= sizeof(tmp)) /* too big, no */
    return -1;
  strlcpy(tmp, start, s-start+1);

  if (tor_version_parse(tmp, router_version)<0) {
    log_info(LD_DIR,"Router version '%s' unparseable.",tmp);
    return -1;
  }

  if (strict) {
    if (router_version->major < 0 ||
        router_version->minor < 0 ||
        router_version->micro < 0 ||
        router_version->patchlevel < 0 ||
        router_version->svn_revision < 0) {
      return -1;
    }
  }

  return 1;
}

/** Parse the Tor version of the platform string <b>platform</b>,
 * and compare it to the version in <b>cutoff</b>. Return 1 if
 * the router is at least as new as the cutoff, else return 0.
 */
int
tor_version_as_new_as(const char *platform, const char *cutoff)
{
  tor_version_t cutoff_version, router_version;
  int r;
  tor_assert(platform);

  if (tor_version_parse(cutoff, &cutoff_version)<0) {
    log_warn(LD_BUG,"cutoff version '%s' unparseable.",cutoff);
    return 0;
  }

  r = tor_version_parse_platform(platform, &router_version, 0);
  if (r == 0) {
    /* nonstandard Tor; be safe and say yes */
    return 1;
  } else if (r < 0) {
    /* unparseable version; be safe and say yes. */
    return 1;
  }

  /* Here's why we don't need to do any special handling for svn revisions:
   * - If neither has an svn revision, we're fine.
   * - If the router doesn't have an svn revision, we can't assume that it
   *   is "at least" any svn revision, so we need to return 0.
   * - If the target version doesn't have an svn revision, any svn revision
   *   (or none at all) is good enough, so return 1.
   * - If both target and router have an svn revision, we compare them.
   */

  return tor_version_compare(&router_version, &cutoff_version) >= 0;
}

/** Parse a tor version from <b>s</b>, and store the result in <b>out</b>.
 * Return 0 on success, -1 on failure. */
int
tor_version_parse(const char *s, tor_version_t *out)
{
  char *eos=NULL;
  const char *cp=NULL;
  int ok = 1;
  /* Format is:
   *   "Tor " ? NUM dot NUM [ dot NUM [ ( pre | rc | dot ) NUM ] ] [ - tag ]
   */
  tor_assert(s);
  tor_assert(out);

  memset(out, 0, sizeof(tor_version_t));
  out->status = VER_RELEASE;
  if (!strcasecmpstart(s, "Tor "))
    s += 4;

  cp = s;

#define NUMBER(m)                               \
  do {                                          \
    if (!cp || *cp < '0' || *cp > '9')          \
      return -1;                                \
    out->m = (int)tor_parse_uint64(cp, 10, 0, INT32_MAX, &ok, &eos);    \
    if (!ok)                                    \
      return -1;                                \
    if (!eos || eos == cp)                      \
      return -1;                                \
    cp = eos;                                   \
  } while (0)

#define DOT()                                   \
  do {                                          \
    if (*cp != '.')                             \
      return -1;                                \
    ++cp;                                       \
  } while (0)

  NUMBER(major);
  DOT();
  NUMBER(minor);
  if (*cp == 0)
    return 0;
  else if (*cp == '-')
    goto status_tag;
  DOT();
  NUMBER(micro);

  /* Get status */
  if (*cp == 0) {
    return 0;
  } else if (*cp == '.') {
    ++cp;
  } else if (*cp == '-') {
    goto status_tag;
  } else if (0==strncmp(cp, "pre", 3)) {
    out->status = VER_PRE;
    cp += 3;
  } else if (0==strncmp(cp, "rc", 2)) {
    out->status = VER_RC;
    cp += 2;
  } else {
    return -1;
  }

  NUMBER(patchlevel);

 status_tag:
  /* Get status tag. */
  if (*cp == '-' || *cp == '.')
    ++cp;
  eos = (char*) find_whitespace(cp);
  if (eos-cp >= (int)sizeof(out->status_tag))
    strlcpy(out->status_tag, cp, sizeof(out->status_tag));
  else {
    memcpy(out->status_tag, cp, eos-cp);
    out->status_tag[eos-cp] = 0;
  }
  cp = eat_whitespace(eos);

  if (!strcmpstart(cp, "(r")) {
    cp += 2;
    out->svn_revision = (int) strtol(cp,&eos,10);
  } else if (!strcmpstart(cp, "(git-")) {
    char *close_paren = strchr(cp, ')');
    int hexlen;
    char digest[DIGEST_LEN];
    if (! close_paren)
      return -1;
    cp += 5;
    if (close_paren-cp > HEX_DIGEST_LEN)
      return -1;
    hexlen = (int)(close_paren-cp);
    memwipe(digest, 0, sizeof(digest));
    if ( hexlen == 0 || (hexlen % 2) == 1)
      return -1;
    if (base16_decode(digest, hexlen/2, cp, hexlen) != hexlen/2)
      return -1;
    memcpy(out->git_tag, digest, hexlen/2);
    out->git_tag_len = hexlen/2;
  }

  return 0;
#undef NUMBER
#undef DOT
}

/** Compare two tor versions; Return <0 if a < b; 0 if a ==b, >0 if a >
 * b. */
int
tor_version_compare(tor_version_t *a, tor_version_t *b)
{
  int i;
  tor_assert(a);
  tor_assert(b);

  /* We take this approach to comparison to ensure the same (bogus!) behavior
   * on all inputs as we would have seen before bug #21278 was fixed. The
   * only important difference here is that this method doesn't cause
   * a signed integer underflow.
   */
#define CMP(field) do {                               \
    unsigned aval = (unsigned) a->field;              \
    unsigned bval = (unsigned) b->field;              \
    int result = (int) (aval - bval);                 \
    if (result < 0)                                   \
      return -1;                                      \
    else if (result > 0)                              \
      return 1;                                       \
  } while (0)

  CMP(major);
  CMP(minor);
  CMP(micro);
  CMP(status);
  CMP(patchlevel);
  if ((i = strcmp(a->status_tag, b->status_tag)))
     return i;
  CMP(svn_revision);
  CMP(git_tag_len);
  if (a->git_tag_len)
     return fast_memcmp(a->git_tag, b->git_tag, a->git_tag_len);
  else
     return 0;

#undef CMP
}

/** Return true iff versions <b>a</b> and <b>b</b> belong to the same series.
 */
int
tor_version_same_series(tor_version_t *a, tor_version_t *b)
{
  tor_assert(a);
  tor_assert(b);
  return ((a->major == b->major) &&
          (a->minor == b->minor) &&
          (a->micro == b->micro));
}

/** Helper: Given pointers to two strings describing tor versions, return -1
 * if _a precedes _b, 1 if _b precedes _a, and 0 if they are equivalent.
 * Used to sort a list of versions. */
static int
compare_tor_version_str_ptr_(const void **_a, const void **_b)
{
  const char *a = *_a, *b = *_b;
  int ca, cb;
  tor_version_t va, vb;
  ca = tor_version_parse(a, &va);
  cb = tor_version_parse(b, &vb);
  /* If they both parse, compare them. */
  if (!ca && !cb)
    return tor_version_compare(&va,&vb);
  /* If one parses, it comes first. */
  if (!ca && cb)
    return -1;
  if (ca && !cb)
    return 1;
  /* If neither parses, compare strings.  Also, the directory server admin
  ** needs to be smacked upside the head.  But Tor is tolerant and gentle. */
  return strcmp(a,b);
}

/** Sort a list of string-representations of versions in ascending order. */
void
sort_version_list(smartlist_t *versions, int remove_duplicates)
{
  smartlist_sort(versions, compare_tor_version_str_ptr_);

  if (remove_duplicates)
    smartlist_uniq(versions, compare_tor_version_str_ptr_, tor_free_);
}

/** Parse and validate the ASCII-encoded v2 descriptor in <b>desc</b>,
 * write the parsed descriptor to the newly allocated *<b>parsed_out</b>, the
 * binary descriptor ID of length DIGEST_LEN to <b>desc_id_out</b>, the
 * encrypted introduction points to the newly allocated
 * *<b>intro_points_encrypted_out</b>, their encrypted size to
 * *<b>intro_points_encrypted_size_out</b>, the size of the encoded descriptor
 * to *<b>encoded_size_out</b>, and a pointer to the possibly next
 * descriptor to *<b>next_out</b>; return 0 for success (including validation)
 * and -1 for failure.
 *
 * If <b>as_hsdir</b> is 1, we're parsing this as an HSDir, and we should
 * be strict about time formats.
 */
int
rend_parse_v2_service_descriptor(rend_service_descriptor_t **parsed_out,
                                 char *desc_id_out,
                                 char **intro_points_encrypted_out,
                                 size_t *intro_points_encrypted_size_out,
                                 size_t *encoded_size_out,
                                 const char **next_out, const char *desc,
                                 int as_hsdir)
{
  rend_service_descriptor_t *result =
                            tor_malloc_zero(sizeof(rend_service_descriptor_t));
  char desc_hash[DIGEST_LEN];
  const char *eos;
  smartlist_t *tokens = smartlist_new();
  directory_token_t *tok;
  char secret_id_part[DIGEST_LEN];
  int i, version, num_ok=1;
  smartlist_t *versions;
  char public_key_hash[DIGEST_LEN];
  char test_desc_id[DIGEST_LEN];
  memarea_t *area = NULL;
  const int strict_time_fmt = as_hsdir;

  tor_assert(desc);
  /* Check if desc starts correctly. */
  if (strncmp(desc, "rendezvous-service-descriptor ",
              strlen("rendezvous-service-descriptor "))) {
    log_info(LD_REND, "Descriptor does not start correctly.");
    goto err;
  }
  /* Compute descriptor hash for later validation. */
  if (router_get_hash_impl(desc, strlen(desc), desc_hash,
                           "rendezvous-service-descriptor ",
                           "\nsignature", '\n', DIGEST_SHA1) < 0) {
    log_warn(LD_REND, "Couldn't compute descriptor hash.");
    goto err;
  }
  /* Determine end of string. */
  eos = strstr(desc, "\nrendezvous-service-descriptor ");
  if (!eos)
    eos = desc + strlen(desc);
  else
    eos = eos + 1;
  /* Check length. */
  if (eos-desc > REND_DESC_MAX_SIZE) {
    /* XXXX+ If we are parsing this descriptor as a server, this
     * should be a protocol warning. */
    log_warn(LD_REND, "Descriptor length is %d which exceeds "
             "maximum rendezvous descriptor size of %d bytes.",
             (int)(eos-desc), REND_DESC_MAX_SIZE);
    goto err;
  }
  /* Tokenize descriptor. */
  area = memarea_new();
  if (tokenize_string(area, desc, eos, tokens, desc_token_table, 0)) {
    log_warn(LD_REND, "Error tokenizing descriptor.");
    goto err;
  }
  /* Set next to next descriptor, if available. */
  *next_out = eos;
  /* Set length of encoded descriptor. */
  *encoded_size_out = eos - desc;
  /* Check min allowed length of token list. */
  if (smartlist_len(tokens) < 7) {
    log_warn(LD_REND, "Impossibly short descriptor.");
    goto err;
  }
  /* Parse base32-encoded descriptor ID. */
  tok = find_by_keyword(tokens, R_RENDEZVOUS_SERVICE_DESCRIPTOR);
  tor_assert(tok == smartlist_get(tokens, 0));
  tor_assert(tok->n_args == 1);
  if (!rend_valid_descriptor_id(tok->args[0])) {
    log_warn(LD_REND, "Invalid descriptor ID: '%s'", tok->args[0]);
    goto err;
  }
  if (base32_decode(desc_id_out, DIGEST_LEN,
                    tok->args[0], REND_DESC_ID_V2_LEN_BASE32) < 0) {
    log_warn(LD_REND, "Descriptor ID contains illegal characters: %s",
             tok->args[0]);
    goto err;
  }
  /* Parse descriptor version. */
  tok = find_by_keyword(tokens, R_VERSION);
  tor_assert(tok->n_args == 1);
  result->version =
    (int) tor_parse_long(tok->args[0], 10, 0, INT_MAX, &num_ok, NULL);
  if (result->version != 2 || !num_ok) {
    /* If it's <2, it shouldn't be under this format.  If the number
     * is greater than 2, we bumped it because we broke backward
     * compatibility.  See how version numbers in our other formats
     * work. */
    log_warn(LD_REND, "Unrecognized descriptor version: %s",
             escaped(tok->args[0]));
    goto err;
  }
  /* Parse public key. */
  tok = find_by_keyword(tokens, R_PERMANENT_KEY);
  result->pk = tok->key;
  tok->key = NULL; /* Prevent free */
  /* Parse secret ID part. */
  tok = find_by_keyword(tokens, R_SECRET_ID_PART);
  tor_assert(tok->n_args == 1);
  if (strlen(tok->args[0]) != REND_SECRET_ID_PART_LEN_BASE32 ||
      strspn(tok->args[0], BASE32_CHARS) != REND_SECRET_ID_PART_LEN_BASE32) {
    log_warn(LD_REND, "Invalid secret ID part: '%s'", tok->args[0]);
    goto err;
  }
  if (base32_decode(secret_id_part, DIGEST_LEN, tok->args[0], 32) < 0) {
    log_warn(LD_REND, "Secret ID part contains illegal characters: %s",
             tok->args[0]);
    goto err;
  }
  /* Parse publication time -- up-to-date check is done when storing the
   * descriptor. */
  tok = find_by_keyword(tokens, R_PUBLICATION_TIME);
  tor_assert(tok->n_args == 1);
  if (parse_iso_time_(tok->args[0], &result->timestamp, strict_time_fmt) < 0) {
    log_warn(LD_REND, "Invalid publication time: '%s'", tok->args[0]);
    goto err;
  }
  /* Parse protocol versions. */
  tok = find_by_keyword(tokens, R_PROTOCOL_VERSIONS);
  tor_assert(tok->n_args == 1);
  versions = smartlist_new();
  smartlist_split_string(versions, tok->args[0], ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  for (i = 0; i < smartlist_len(versions); i++) {
    version = (int) tor_parse_long(smartlist_get(versions, i),
                                   10, 0, INT_MAX, &num_ok, NULL);
    if (!num_ok) /* It's a string; let's ignore it. */
      continue;
    if (version >= REND_PROTOCOL_VERSION_BITMASK_WIDTH)
      /* Avoid undefined left-shift behaviour. */
      continue;
    result->protocols |= 1 << version;
  }
  SMARTLIST_FOREACH(versions, char *, cp, tor_free(cp));
  smartlist_free(versions);
  /* Parse encrypted introduction points. Don't verify. */
  tok = find_opt_by_keyword(tokens, R_INTRODUCTION_POINTS);
  if (tok) {
    if (strcmp(tok->object_type, "MESSAGE")) {
      log_warn(LD_DIR, "Bad object type: introduction points should be of "
               "type MESSAGE");
      goto err;
    }
    *intro_points_encrypted_out = tor_memdup(tok->object_body,
                                             tok->object_size);
    *intro_points_encrypted_size_out = tok->object_size;
  } else {
    *intro_points_encrypted_out = NULL;
    *intro_points_encrypted_size_out = 0;
  }
  /* Parse and verify signature. */
  tok = find_by_keyword(tokens, R_SIGNATURE);
  note_crypto_pk_op(VERIFY_RTR);
  if (check_signature_token(desc_hash, DIGEST_LEN, tok, result->pk, 0,
                            "v2 rendezvous service descriptor") < 0)
    goto err;
  /* Verify that descriptor ID belongs to public key and secret ID part. */
  crypto_pk_get_digest(result->pk, public_key_hash);
  rend_get_descriptor_id_bytes(test_desc_id, public_key_hash,
                               secret_id_part);
  if (tor_memneq(desc_id_out, test_desc_id, DIGEST_LEN)) {
    log_warn(LD_REND, "Parsed descriptor ID does not match "
             "computed descriptor ID.");
    goto err;
  }
  goto done;
 err:
  rend_service_descriptor_free(result);
  result = NULL;
 done:
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  if (area)
    memarea_drop_all(area);
  *parsed_out = result;
  if (result)
    return 0;
  return -1;
}

/** Decrypt the encrypted introduction points in <b>ipos_encrypted</b> of
 * length <b>ipos_encrypted_size</b> using <b>descriptor_cookie</b> and
 * write the result to a newly allocated string that is pointed to by
 * <b>ipos_decrypted</b> and its length to <b>ipos_decrypted_size</b>.
 * Return 0 if decryption was successful and -1 otherwise. */
int
rend_decrypt_introduction_points(char **ipos_decrypted,
                                 size_t *ipos_decrypted_size,
                                 const char *descriptor_cookie,
                                 const char *ipos_encrypted,
                                 size_t ipos_encrypted_size)
{
  tor_assert(ipos_encrypted);
  tor_assert(descriptor_cookie);
  if (ipos_encrypted_size < 2) {
    log_warn(LD_REND, "Size of encrypted introduction points is too "
                      "small.");
    return -1;
  }
  if (ipos_encrypted[0] == (int)REND_BASIC_AUTH) {
    char iv[CIPHER_IV_LEN], client_id[REND_BASIC_AUTH_CLIENT_ID_LEN],
         session_key[CIPHER_KEY_LEN], *dec;
    int declen, client_blocks;
    size_t pos = 0, len, client_entries_len;
    crypto_digest_t *digest;
    crypto_cipher_t *cipher;
    client_blocks = (int) ipos_encrypted[1];
    client_entries_len = client_blocks * REND_BASIC_AUTH_CLIENT_MULTIPLE *
                         REND_BASIC_AUTH_CLIENT_ENTRY_LEN;
    if (ipos_encrypted_size < 2 + client_entries_len + CIPHER_IV_LEN + 1) {
      log_warn(LD_REND, "Size of encrypted introduction points is too "
                        "small.");
      return -1;
    }
    memcpy(iv, ipos_encrypted + 2 + client_entries_len, CIPHER_IV_LEN);
    digest = crypto_digest_new();
    crypto_digest_add_bytes(digest, descriptor_cookie, REND_DESC_COOKIE_LEN);
    crypto_digest_add_bytes(digest, iv, CIPHER_IV_LEN);
    crypto_digest_get_digest(digest, client_id,
                             REND_BASIC_AUTH_CLIENT_ID_LEN);
    crypto_digest_free(digest);
    for (pos = 2; pos < 2 + client_entries_len;
         pos += REND_BASIC_AUTH_CLIENT_ENTRY_LEN) {
      if (tor_memeq(ipos_encrypted + pos, client_id,
                  REND_BASIC_AUTH_CLIENT_ID_LEN)) {
        /* Attempt to decrypt introduction points. */
        cipher = crypto_cipher_new(descriptor_cookie);
        if (crypto_cipher_decrypt(cipher, session_key, ipos_encrypted
                                  + pos + REND_BASIC_AUTH_CLIENT_ID_LEN,
                                  CIPHER_KEY_LEN) < 0) {
          log_warn(LD_REND, "Could not decrypt session key for client.");
          crypto_cipher_free(cipher);
          return -1;
        }
        crypto_cipher_free(cipher);

        len = ipos_encrypted_size - 2 - client_entries_len - CIPHER_IV_LEN;
        dec = tor_malloc_zero(len + 1);
        declen = crypto_cipher_decrypt_with_iv(session_key, dec, len,
            ipos_encrypted + 2 + client_entries_len,
            ipos_encrypted_size - 2 - client_entries_len);

        if (declen < 0) {
          log_warn(LD_REND, "Could not decrypt introduction point string.");
          tor_free(dec);
          return -1;
        }
        if (fast_memcmpstart(dec, declen, "introduction-point ")) {
          log_warn(LD_REND, "Decrypted introduction points don't "
                            "look like we could parse them.");
          tor_free(dec);
          continue;
        }
        *ipos_decrypted = dec;
        *ipos_decrypted_size = declen;
        return 0;
      }
    }
    log_warn(LD_REND, "Could not decrypt introduction points. Please "
             "check your authorization for this service!");
    return -1;
  } else if (ipos_encrypted[0] == (int)REND_STEALTH_AUTH) {
    char *dec;
    int declen;
    if (ipos_encrypted_size < CIPHER_IV_LEN + 2) {
      log_warn(LD_REND, "Size of encrypted introduction points is too "
                        "small.");
      return -1;
    }
    dec = tor_malloc_zero(ipos_encrypted_size - CIPHER_IV_LEN - 1 + 1);

    declen = crypto_cipher_decrypt_with_iv(descriptor_cookie, dec,
                                           ipos_encrypted_size -
                                               CIPHER_IV_LEN - 1,
                                           ipos_encrypted + 1,
                                           ipos_encrypted_size - 1);

    if (declen < 0) {
      log_warn(LD_REND, "Decrypting introduction points failed!");
      tor_free(dec);
      return -1;
    }
    *ipos_decrypted = dec;
    *ipos_decrypted_size = declen;
    return 0;
  } else {
    log_warn(LD_REND, "Unknown authorization type number: %d",
             ipos_encrypted[0]);
    return -1;
  }
}

/** Parse the encoded introduction points in <b>intro_points_encoded</b> of
 * length <b>intro_points_encoded_size</b> and write the result to the
 * descriptor in <b>parsed</b>; return the number of successfully parsed
 * introduction points or -1 in case of a failure. */
int
rend_parse_introduction_points(rend_service_descriptor_t *parsed,
                               const char *intro_points_encoded,
                               size_t intro_points_encoded_size)
{
  const char *current_ipo, *end_of_intro_points;
  smartlist_t *tokens = NULL;
  directory_token_t *tok;
  rend_intro_point_t *intro;
  extend_info_t *info;
  int result, num_ok=1;
  memarea_t *area = NULL;
  tor_assert(parsed);
  /** Function may only be invoked once. */
  tor_assert(!parsed->intro_nodes);
  if (!intro_points_encoded || intro_points_encoded_size == 0) {
    log_warn(LD_REND, "Empty or zero size introduction point list");
    goto err;
  }
  /* Consider one intro point after the other. */
  current_ipo = intro_points_encoded;
  end_of_intro_points = intro_points_encoded + intro_points_encoded_size;
  tokens = smartlist_new();
  parsed->intro_nodes = smartlist_new();
  area = memarea_new();

  while (!fast_memcmpstart(current_ipo, end_of_intro_points-current_ipo,
                      "introduction-point ")) {
    /* Determine end of string. */
    const char *eos = tor_memstr(current_ipo, end_of_intro_points-current_ipo,
                                 "\nintroduction-point ");
    if (!eos)
      eos = end_of_intro_points;
    else
      eos = eos+1;
    tor_assert(eos <= intro_points_encoded+intro_points_encoded_size);
    /* Free tokens and clear token list. */
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_clear(tokens);
    memarea_clear(area);
    /* Tokenize string. */
    if (tokenize_string(area, current_ipo, eos, tokens, ipo_token_table, 0)) {
      log_warn(LD_REND, "Error tokenizing introduction point");
      goto err;
    }
    /* Advance to next introduction point, if available. */
    current_ipo = eos;
    /* Check minimum allowed length of introduction point. */
    if (smartlist_len(tokens) < 5) {
      log_warn(LD_REND, "Impossibly short introduction point.");
      goto err;
    }
    /* Allocate new intro point and extend info. */
    intro = tor_malloc_zero(sizeof(rend_intro_point_t));
    info = intro->extend_info = tor_malloc_zero(sizeof(extend_info_t));
    /* Parse identifier. */
    tok = find_by_keyword(tokens, R_IPO_IDENTIFIER);
    if (base32_decode(info->identity_digest, DIGEST_LEN,
                      tok->args[0], REND_INTRO_POINT_ID_LEN_BASE32) < 0) {
      log_warn(LD_REND, "Identity digest contains illegal characters: %s",
               tok->args[0]);
      rend_intro_point_free(intro);
      goto err;
    }
    /* Write identifier to nickname. */
    info->nickname[0] = '$';
    base16_encode(info->nickname + 1, sizeof(info->nickname) - 1,
                  info->identity_digest, DIGEST_LEN);
    /* Parse IP address. */
    tok = find_by_keyword(tokens, R_IPO_IP_ADDRESS);
    if (tor_addr_parse(&info->addr, tok->args[0])<0) {
      log_warn(LD_REND, "Could not parse introduction point address.");
      rend_intro_point_free(intro);
      goto err;
    }
    if (tor_addr_family(&info->addr) != AF_INET) {
      log_warn(LD_REND, "Introduction point address was not ipv4.");
      rend_intro_point_free(intro);
      goto err;
    }

    /* Parse onion port. */
    tok = find_by_keyword(tokens, R_IPO_ONION_PORT);
    info->port = (uint16_t) tor_parse_long(tok->args[0],10,1,65535,
                                           &num_ok,NULL);
    if (!info->port || !num_ok) {
      log_warn(LD_REND, "Introduction point onion port %s is invalid",
               escaped(tok->args[0]));
      rend_intro_point_free(intro);
      goto err;
    }
    /* Parse onion key. */
    tok = find_by_keyword(tokens, R_IPO_ONION_KEY);
    if (!crypto_pk_public_exponent_ok(tok->key)) {
      log_warn(LD_REND,
               "Introduction point's onion key had invalid exponent.");
      rend_intro_point_free(intro);
      goto err;
    }
    info->onion_key = tok->key;
    tok->key = NULL; /* Prevent free */
    /* Parse service key. */
    tok = find_by_keyword(tokens, R_IPO_SERVICE_KEY);
    if (!crypto_pk_public_exponent_ok(tok->key)) {
      log_warn(LD_REND,
               "Introduction point key had invalid exponent.");
      rend_intro_point_free(intro);
      goto err;
    }
    intro->intro_key = tok->key;
    tok->key = NULL; /* Prevent free */
    /* Add extend info to list of introduction points. */
    smartlist_add(parsed->intro_nodes, intro);
  }
  result = smartlist_len(parsed->intro_nodes);
  goto done;

 err:
  result = -1;

 done:
  /* Free tokens and clear token list. */
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  if (area)
    memarea_drop_all(area);

  return result;
}

/** Parse the content of a client_key file in <b>ckstr</b> and add
 * rend_authorized_client_t's for each parsed client to
 * <b>parsed_clients</b>. Return the number of parsed clients as result
 * or -1 for failure. */
int
rend_parse_client_keys(strmap_t *parsed_clients, const char *ckstr)
{
  int result = -1;
  smartlist_t *tokens;
  directory_token_t *tok;
  const char *current_entry = NULL;
  memarea_t *area = NULL;
  char *err_msg = NULL;
  if (!ckstr || strlen(ckstr) == 0)
    return -1;
  tokens = smartlist_new();
  /* Begin parsing with first entry, skipping comments or whitespace at the
   * beginning. */
  area = memarea_new();
  current_entry = eat_whitespace(ckstr);
  while (!strcmpstart(current_entry, "client-name ")) {
    rend_authorized_client_t *parsed_entry;
    /* Determine end of string. */
    const char *eos = strstr(current_entry, "\nclient-name ");
    if (!eos)
      eos = current_entry + strlen(current_entry);
    else
      eos = eos + 1;
    /* Free tokens and clear token list. */
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_clear(tokens);
    memarea_clear(area);
    /* Tokenize string. */
    if (tokenize_string(area, current_entry, eos, tokens,
                        client_keys_token_table, 0)) {
      log_warn(LD_REND, "Error tokenizing client keys file.");
      goto err;
    }
    /* Advance to next entry, if available. */
    current_entry = eos;
    /* Check minimum allowed length of token list. */
    if (smartlist_len(tokens) < 2) {
      log_warn(LD_REND, "Impossibly short client key entry.");
      goto err;
    }
    /* Parse client name. */
    tok = find_by_keyword(tokens, C_CLIENT_NAME);
    tor_assert(tok == smartlist_get(tokens, 0));
    tor_assert(tok->n_args == 1);

    if (!rend_valid_client_name(tok->args[0])) {
      log_warn(LD_CONFIG, "Illegal client name: %s. (Length must be "
               "between 1 and %d, and valid characters are "
               "[A-Za-z0-9+-_].)", tok->args[0], REND_CLIENTNAME_MAX_LEN);
      goto err;
    }
    /* Check if client name is duplicate. */
    if (strmap_get(parsed_clients, tok->args[0])) {
      log_warn(LD_CONFIG, "HiddenServiceAuthorizeClient contains a "
               "duplicate client name: '%s'. Ignoring.", tok->args[0]);
      goto err;
    }
    parsed_entry = tor_malloc_zero(sizeof(rend_authorized_client_t));
    parsed_entry->client_name = tor_strdup(tok->args[0]);
    strmap_set(parsed_clients, parsed_entry->client_name, parsed_entry);
    /* Parse client key. */
    tok = find_opt_by_keyword(tokens, C_CLIENT_KEY);
    if (tok) {
      parsed_entry->client_key = tok->key;
      tok->key = NULL; /* Prevent free */
    }

    /* Parse descriptor cookie. */
    tok = find_by_keyword(tokens, C_DESCRIPTOR_COOKIE);
    tor_assert(tok->n_args == 1);
    if (rend_auth_decode_cookie(tok->args[0], parsed_entry->descriptor_cookie,
                                NULL, &err_msg) < 0) {
      tor_assert(err_msg);
      log_warn(LD_REND, "%s", err_msg);
      tor_free(err_msg);
      goto err;
    }
  }
  result = strmap_size(parsed_clients);
  goto done;
 err:
  result = -1;
 done:
  /* Free tokens and clear token list. */
  SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
  smartlist_free(tokens);
  if (area)
    memarea_drop_all(area);
  return result;
}

/** Called on startup; right now we just handle scanning the unparseable
 * descriptor dumps, but hang anything else we might need to do in the
 * future here as well.
 */
void
routerparse_init(void)
{
  /*
   * Check both if the sandbox is active and whether it's configured; no
   * point in loading all that if we won't be able to use it after the
   * sandbox becomes active.
   */
  if (!(sandbox_is_active() || get_options()->Sandbox)) {
    dump_desc_init();
  }
}

/** Clean up all data structures used by routerparse.c at exit */
void
routerparse_free_all(void)
{
  dump_desc_fifo_cleanup();
}

