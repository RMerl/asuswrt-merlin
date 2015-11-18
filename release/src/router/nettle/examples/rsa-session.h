/* Session key definitions for the rsa-encrypt and rsa-decrypt programs.
 */

#ifndef NETTLE_EXAMPLES_RSA_SESSION_H_INCLUDED
#define NETTLE_EXAMPLES_RSA_SESSION_H_INCLUDED

#include "aes.h"
#include "cbc.h"
#include "hmac.h"

#define RSA_VERSION 1

/* Encryption program using the following file format:

     uint32_t version = 1;
     uint32_t nsize;
     uint8_t x[nsize];
     uint8_t encrypted[n];
     uint8_t hmac[SHA1_DIGEST_SIZE];

   where x is the data

     uint32_t version = 1;
     uint8_t aes_key[AES_KEY_SIZE];
     uint8_t iv[AES_BLOCK_SIZE];
     uint8_t hmac_key[SHA1_DIGEST_SIZE];

   of size (4 + AES_KEY_SIZE + AES_BLOCK_SIZE + SHA1_DIGEST_SIZE) = 72
   bytes, encrypted using rsa-pkcs1.

   The cleartext input is encrypted using aes-cbc. The final block is
   padded as

     | data | random octets | padding length |

   where the last octet is the padding length, a number between 1 and
   AES_BLOCK_SIZE (inclusive).
*/

struct rsa_session
{
  struct CBC_CTX(struct aes_ctx, AES_BLOCK_SIZE) aes;
  struct hmac_sha1_ctx hmac;
  struct yarrow256_ctx yarrow;
};

struct rsa_session_info
{
  /* Version followed by aes key, iv and mac key */
  uint8_t key[4 + AES_KEY_SIZE + AES_BLOCK_SIZE + SHA1_DIGEST_SIZE];
};

#define SESSION_VERSION(s) ((s)->key)
#define SESSION_AES_KEY(s) ((s)->key + 4)
#define SESSION_IV(s) ((s)->key + 4 + AES_KEY_SIZE)
#define SESSION_HMAC_KEY(s) ((s)->key + 4 + AES_KEY_SIZE + AES_BLOCK_SIZE)

void
rsa_session_set_encrypt_key(struct rsa_session *ctx,
			    const struct rsa_session_info *key);

void
rsa_session_set_decrypt_key(struct rsa_session *ctx,
			    const struct rsa_session_info *key);

#endif /* NETTLE_EXAMPLES_RSA_SESSION_H_INCLUDED */
