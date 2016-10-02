/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_COMPAT_OPENSSL_H
#define TOR_COMPAT_OPENSSL_H

#include <openssl/opensslv.h>

/**
 * \file compat_openssl.h
 *
 * \brief compatability definitions for working with different openssl forks
 **/

#if OPENSSL_VERSION_NUMBER < OPENSSL_V_SERIES(1,0,0)
#error "We require OpenSSL >= 1.0.0"
#endif

#if OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,1,0) && \
   ! defined(LIBRESSL_VERSION_NUMBER)
/* We define this macro if we're trying to build with the majorly refactored
 * API in OpenSSL 1.1 */
#define OPENSSL_1_1_API
#endif

#ifndef OPENSSL_1_1_API
#define OPENSSL_VERSION SSLEAY_VERSION
#define OpenSSL_version(v) SSLeay_version(v)
#define OpenSSL_version_num() SSLeay()
#define RAND_OpenSSL() RAND_SSLeay()
#define STATE_IS_SW_SERVER_HELLO(st)       \
  (((st) == SSL3_ST_SW_SRVR_HELLO_A) ||    \
   ((st) == SSL3_ST_SW_SRVR_HELLO_B))
#define OSSL_HANDSHAKE_STATE int
#define CONST_IF_OPENSSL_1_1_API
#else
#define STATE_IS_SW_SERVER_HELLO(st) \
  ((st) == TLS_ST_SW_SRVR_HELLO)
#define CONST_IF_OPENSSL_1_1_API const
#endif

#endif

