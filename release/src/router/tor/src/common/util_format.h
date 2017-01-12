/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_UTIL_FORMAT_H
#define TOR_UTIL_FORMAT_H

#include "testsupport.h"
#include "torint.h"

#define BASE64_ENCODE_MULTILINE 1
size_t base64_encode_size(size_t srclen, int flags);
int base64_encode(char *dest, size_t destlen, const char *src, size_t srclen,
                  int flags);
int base64_decode(char *dest, size_t destlen, const char *src, size_t srclen);
int base64_encode_nopad(char *dest, size_t destlen,
                        const uint8_t *src, size_t srclen);
int base64_decode_nopad(uint8_t *dest, size_t destlen,
                        const char *src, size_t srclen);

/** Characters that can appear (case-insensitively) in a base32 encoding. */
#define BASE32_CHARS "abcdefghijklmnopqrstuvwxyz234567"
void base32_encode(char *dest, size_t destlen, const char *src, size_t srclen);
int base32_decode(char *dest, size_t destlen, const char *src, size_t srclen);
size_t base32_encoded_size(size_t srclen);

int hex_decode_digit(char c);
void base16_encode(char *dest, size_t destlen, const char *src, size_t srclen);
int base16_decode(char *dest, size_t destlen, const char *src, size_t srclen);

#endif

