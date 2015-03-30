/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file onion.h
 * \brief Header file for onion.c.
 **/

#ifndef TOR_ONION_H
#define TOR_ONION_H

struct create_cell_t;
int onion_pending_add(or_circuit_t *circ, struct create_cell_t *onionskin);
or_circuit_t *onion_next_task(struct create_cell_t **onionskin_out);
int onion_num_pending(uint16_t handshake_type);
void onion_pending_remove(or_circuit_t *circ);
void clear_pending_onions(void);

typedef struct server_onion_keys_t {
  uint8_t my_identity[DIGEST_LEN];
  crypto_pk_t *onion_key;
  crypto_pk_t *last_onion_key;
#ifdef CURVE25519_ENABLED
  di_digest256_map_t *curve25519_key_map;
  curve25519_keypair_t *junk_keypair;
#endif
} server_onion_keys_t;

#define MAX_ONIONSKIN_CHALLENGE_LEN 255
#define MAX_ONIONSKIN_REPLY_LEN 255

void setup_server_onion_keys(server_onion_keys_t *keys);
void release_server_onion_keys(server_onion_keys_t *keys);

void onion_handshake_state_release(onion_handshake_state_t *state);

int onion_skin_create(int type,
                      const extend_info_t *node,
                      onion_handshake_state_t *state_out,
                      uint8_t *onion_skin_out);
int onion_skin_server_handshake(int type,
                      const uint8_t *onion_skin, size_t onionskin_len,
                      const server_onion_keys_t *keys,
                      uint8_t *reply_out,
                      uint8_t *keys_out, size_t key_out_len,
                      uint8_t *rend_nonce_out);
int onion_skin_client_handshake(int type,
                      const onion_handshake_state_t *handshake_state,
                      const uint8_t *reply, size_t reply_len,
                      uint8_t *keys_out, size_t key_out_len,
                      uint8_t *rend_authenticator_out);

/** A parsed CREATE, CREATE_FAST, or CREATE2 cell. */
typedef struct create_cell_t {
  /** The cell command. One of CREATE{,_FAST,2} */
  uint8_t cell_type;
  /** One of the ONION_HANDSHAKE_TYPE_* values */
  uint16_t handshake_type;
  /** The number of bytes used in <b>onionskin</b>. */
  uint16_t handshake_len;
  /** The client-side message for the circuit creation handshake. */
  uint8_t onionskin[CELL_PAYLOAD_SIZE - 4];
} create_cell_t;

/** A parsed CREATED, CREATED_FAST, or CREATED2 cell. */
typedef struct created_cell_t {
  /** The cell command. One of CREATED{,_FAST,2} */
  uint8_t cell_type;
  /** The number of bytes used in <b>reply</b>. */
  uint16_t handshake_len;
  /** The server-side message for the circuit creation handshake. */
  uint8_t reply[CELL_PAYLOAD_SIZE - 2];
} created_cell_t;

/** A parsed RELAY_EXTEND or RELAY_EXTEND2 cell */
typedef struct extend_cell_t {
  /** One of RELAY_EXTEND or RELAY_EXTEND2 */
  uint8_t cell_type;
  /** An IPv4 address and port for the node we're connecting to. */
  tor_addr_port_t orport_ipv4;
  /** An IPv6 address and port for the node we're connecting to. Not currently
   * used. */
  tor_addr_port_t orport_ipv6;
  /** Identity fingerprint of the node we're conecting to.*/
  uint8_t node_id[DIGEST_LEN];
  /** The "create cell" embedded in this extend cell. Note that unlike the
   * create cells we generate ourself, this once can have a handshake type we
   * don't recognize. */
  create_cell_t create_cell;
} extend_cell_t;

/** A parsed RELAY_EXTEND or RELAY_EXTEND2 cell */
typedef struct extended_cell_t {
  /** One of RELAY_EXTENDED or RELAY_EXTENDED2. */
  uint8_t cell_type;
  /** The "created cell" embedded in this extended cell. */
  created_cell_t created_cell;
} extended_cell_t;

void create_cell_init(create_cell_t *cell_out, uint8_t cell_type,
                      uint16_t handshake_type, uint16_t handshake_len,
                      const uint8_t *onionskin);
int create_cell_parse(create_cell_t *cell_out, const cell_t *cell_in);
int created_cell_parse(created_cell_t *cell_out, const cell_t *cell_in);
int extend_cell_parse(extend_cell_t *cell_out, const uint8_t command,
                      const uint8_t *payload_in, size_t payload_len);
int extended_cell_parse(extended_cell_t *cell_out, const uint8_t command,
                        const uint8_t *payload_in, size_t payload_len);

int create_cell_format(cell_t *cell_out, const create_cell_t *cell_in);
int create_cell_format_relayed(cell_t *cell_out, const create_cell_t *cell_in);
int created_cell_format(cell_t *cell_out, const created_cell_t *cell_in);
int extend_cell_format(uint8_t *command_out, uint16_t *len_out,
                       uint8_t *payload_out, const extend_cell_t *cell_in);
int extended_cell_format(uint8_t *command_out, uint16_t *len_out,
                         uint8_t *payload_out, const extended_cell_t *cell_in);

#endif

