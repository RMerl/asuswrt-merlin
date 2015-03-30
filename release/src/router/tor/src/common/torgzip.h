/* Copyright (c) 2003, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file torgzip.h
 * \brief Headers for torgzip.h
 **/

#ifndef TOR_TORGZIP_H
#define TOR_TORGZIP_H

/** Enumeration of what kind of compression to use.  Only ZLIB_METHOD is
 * guaranteed to be supported by the compress/uncompress functions here;
 * GZIP_METHOD may be supported if we built against zlib version 1.2 or later
 * and is_gzip_supported() returns true. */
typedef enum {
  NO_METHOD=0, GZIP_METHOD=1, ZLIB_METHOD=2, UNKNOWN_METHOD=3
} compress_method_t;

int
tor_gzip_compress(char **out, size_t *out_len,
                  const char *in, size_t in_len,
                  compress_method_t method);
int
tor_gzip_uncompress(char **out, size_t *out_len,
                    const char *in, size_t in_len,
                    compress_method_t method,
                    int complete_only,
                    int protocol_warn_level);

int is_gzip_supported(void);

const char *
tor_zlib_get_version_str(void);

const char *
tor_zlib_get_header_version_str(void);

compress_method_t detect_compression_method(const char *in, size_t in_len);

/** Return values from tor_zlib_process; see that function's documentation for
 * details. */
typedef enum {
  TOR_ZLIB_OK, TOR_ZLIB_DONE, TOR_ZLIB_BUF_FULL, TOR_ZLIB_ERR
} tor_zlib_output_t;
/** Internal state for an incremental zlib compression/decompression. */
typedef struct tor_zlib_state_t tor_zlib_state_t;
tor_zlib_state_t *tor_zlib_new(int compress, compress_method_t method);

tor_zlib_output_t tor_zlib_process(tor_zlib_state_t *state,
                                   char **out, size_t *out_len,
                                   const char **in, size_t *in_len,
                                   int finish);
void tor_zlib_free(tor_zlib_state_t *state);

#endif

