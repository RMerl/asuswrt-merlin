/* Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include <stdio.h>
#include <stdlib.h>

#define ONION_NTOR_PRIVATE
#include "or.h"
#include "util.h"
#include "compat.h"
#include "crypto.h"
#include "crypto_curve25519.h"
#include "onion_ntor.h"

#ifndef CURVE25519_ENABLED
#error "This isn't going to work without curve25519."
#endif

#define N_ARGS(n) STMT_BEGIN {                                  \
    if (argc < (n)) {                                           \
      fprintf(stderr, "%s needs %d arguments.\n",argv[1],n);    \
      return 1;                                                 \
    }                                                           \
  } STMT_END
#define BASE16(idx, var, n) STMT_BEGIN {                                \
    const char *s = argv[(idx)];                                        \
    if (base16_decode((char*)var, n, s, strlen(s)) < 0 ) {              \
      fprintf(stderr, "couldn't decode argument %d (%s)\n",idx,s);      \
      return 1;                                                         \
    }                                                                   \
  } STMT_END
#define INT(idx, var) STMT_BEGIN {                                      \
    var = atoi(argv[(idx)]);                                            \
    if (var <= 0) {                                                     \
      fprintf(stderr, "bad integer argument %d (%s)\n",idx,argv[(idx)]); \
    }                                                                   \
  } STMT_END

static int
client1(int argc, char **argv)
{
  /* client1 nodeID B -> msg state */
  curve25519_public_key_t B;
  uint8_t node_id[DIGEST_LEN];
  ntor_handshake_state_t *state = NULL;
  uint8_t msg[NTOR_ONIONSKIN_LEN];

  char buf[1024];

  N_ARGS(4);
  BASE16(2, node_id, DIGEST_LEN);
  BASE16(3, B.public_key, CURVE25519_PUBKEY_LEN);

  if (onion_skin_ntor_create(node_id, &B, &state, msg)<0) {
    fprintf(stderr, "handshake failed");
    return 2;
  }

  base16_encode(buf, sizeof(buf), (const char*)msg, sizeof(msg));
  printf("%s\n", buf);
  base16_encode(buf, sizeof(buf), (void*)state, sizeof(*state));
  printf("%s\n", buf);

  ntor_handshake_state_free(state);
  return 0;
}

static int
server1(int argc, char **argv)
{
  uint8_t msg_in[NTOR_ONIONSKIN_LEN];
  curve25519_keypair_t kp;
  di_digest256_map_t *keymap=NULL;
  uint8_t node_id[DIGEST_LEN];
  int keybytes;

  uint8_t msg_out[NTOR_REPLY_LEN];
  uint8_t *keys = NULL;
  char *hexkeys = NULL;
  int result = 0;

  char buf[256];

  /* server1: b nodeID msg N -> msg keys */
  N_ARGS(6);
  BASE16(2, kp.seckey.secret_key, CURVE25519_SECKEY_LEN);
  BASE16(3, node_id, DIGEST_LEN);
  BASE16(4, msg_in, NTOR_ONIONSKIN_LEN);
  INT(5, keybytes);

  curve25519_public_key_generate(&kp.pubkey, &kp.seckey);
  dimap_add_entry(&keymap, kp.pubkey.public_key, &kp);

  keys = tor_malloc(keybytes);
  hexkeys = tor_malloc(keybytes*2+1);
  if (onion_skin_ntor_server_handshake(
                                msg_in, keymap, NULL, node_id, msg_out, keys,
                                (size_t)keybytes)<0) {
    fprintf(stderr, "handshake failed");
    result = 2;
    goto done;
  }

  base16_encode(buf, sizeof(buf), (const char*)msg_out, sizeof(msg_out));
  printf("%s\n", buf);
  base16_encode(hexkeys, keybytes*2+1, (const char*)keys, keybytes);
  printf("%s\n", hexkeys);

 done:
  tor_free(keys);
  tor_free(hexkeys);
  return result;
}

static int
client2(int argc, char **argv)
{
  struct ntor_handshake_state_t state;
  uint8_t msg[NTOR_REPLY_LEN];
  int keybytes;
  uint8_t *keys;
  char *hexkeys;
  int result = 0;

  N_ARGS(5);
  BASE16(2, (&state), sizeof(state));
  BASE16(3, msg, sizeof(msg));
  INT(4, keybytes);

  keys = tor_malloc(keybytes);
  hexkeys = tor_malloc(keybytes*2+1);
  if (onion_skin_ntor_client_handshake(&state, msg, keys, keybytes)<0) {
    fprintf(stderr, "handshake failed");
    result = 2;
    goto done;
  }

  base16_encode(hexkeys, keybytes*2+1, (const char*)keys, keybytes);
  printf("%s\n", hexkeys);

 done:
  tor_free(keys);
  tor_free(hexkeys);
  return result;
}

int
main(int argc, char **argv)
{
  /*
    client1: nodeID B -> msg state
    server1: b nodeID msg N -> msg keys
    client2: state msg N -> keys
  */
  if (argc < 2) {
    fprintf(stderr, "I need arguments. Read source for more info.\n");
    return 1;
  } else if (!strcmp(argv[1], "client1")) {
    return client1(argc, argv);
  } else if (!strcmp(argv[1], "server1")) {
    return server1(argc, argv);
  } else if (!strcmp(argv[1], "client2")) {
    return client2(argc, argv);
  } else {
    fprintf(stderr, "What's a %s?\n", argv[1]);
    return 1;
  }
}

