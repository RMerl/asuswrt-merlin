/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2017 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *  Copyright (C) 2010-2017 Fox Crypto B.V. <openvpn@fox-it.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file Data Channel Cryptography OpenSSL-specific backend interface
 */

#ifndef CRYPTO_OPENSSL_H_
#define CRYPTO_OPENSSL_H_

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

/** Generic cipher key type %context. */
typedef EVP_CIPHER cipher_kt_t;

/** Generic message digest key type %context. */
typedef EVP_MD md_kt_t;

/** Generic cipher %context. */
typedef EVP_CIPHER_CTX cipher_ctx_t;

/** Generic message digest %context. */
typedef EVP_MD_CTX md_ctx_t;

/** Generic HMAC %context. */
typedef HMAC_CTX hmac_ctx_t;

/** Maximum length of an IV */
#define OPENVPN_MAX_IV_LENGTH   EVP_MAX_IV_LENGTH

/** Cipher is in CBC mode */
#define OPENVPN_MODE_CBC        EVP_CIPH_CBC_MODE

/** Cipher is in OFB mode */
#define OPENVPN_MODE_OFB        EVP_CIPH_OFB_MODE

/** Cipher is in CFB mode */
#define OPENVPN_MODE_CFB        EVP_CIPH_CFB_MODE

#ifdef HAVE_AEAD_CIPHER_MODES

/** Cipher is in GCM mode */
#define OPENVPN_MODE_GCM        EVP_CIPH_GCM_MODE

#endif /* HAVE_AEAD_CIPHER_MODES */

/** Cipher should encrypt */
#define OPENVPN_OP_ENCRYPT      1

/** Cipher should decrypt */
#define OPENVPN_OP_DECRYPT      0

#define DES_KEY_LENGTH 8
#define MD4_DIGEST_LENGTH       16

/**
 * Retrieve any occurred OpenSSL errors and print those errors.
 *
 * Note that this function uses the not thread-safe OpenSSL error API.
 *
 * @param flags         Flags to indicate error type and priority.
 */
void crypto_print_openssl_errors(const unsigned int flags);

/**
 * Retrieve any OpenSSL errors, then print the supplied error message.
 *
 * This is just a convenience wrapper for often occurring situations.
 *
+ * @param flags         Flags to indicate error type and priority.
+ * @param format        Format string to print.
+ * @param format args   (optional) arguments for the format string.
 */
#define crypto_msg(flags, ...) \
    do { \
        crypto_print_openssl_errors(nonfatal(flags)); \
        msg((flags | M_SSL), __VA_ARGS__); \
    } while (false)


#endif /* CRYPTO_OPENSSL_H_ */
