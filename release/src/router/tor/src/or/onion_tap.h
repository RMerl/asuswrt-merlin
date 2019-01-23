/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file onion_tap.h
 * \brief Header file for onion_tap.c.
 **/

#ifndef TOR_ONION_TAP_H
#define TOR_ONION_TAP_H

#define TAP_ONIONSKIN_CHALLENGE_LEN (PKCS1_OAEP_PADDING_OVERHEAD+\
                                 CIPHER_KEY_LEN+\
                                 DH_KEY_LEN)
#define TAP_ONIONSKIN_REPLY_LEN (DH_KEY_LEN+DIGEST_LEN)

int onion_skin_TAP_create(crypto_pk_t *router_key,
                      crypto_dh_t **handshake_state_out,
                      char *onion_skin_out);

int onion_skin_TAP_server_handshake(const char *onion_skin,
                                crypto_pk_t *private_key,
                                crypto_pk_t *prev_private_key,
                                char *handshake_reply_out,
                                char *key_out,
                                size_t key_out_len);

int onion_skin_TAP_client_handshake(crypto_dh_t *handshake_state,
                                const char *handshake_reply,
                                char *key_out,
                                size_t key_out_len,
                                const char **msg_out);

#endif

