
#include "crypto.h"
#include "crypto_s2k.h"
#include "crypto_pwbox.h"
#include "di_ops.h"
#include "util.h"
#include "pwbox.h"

/* 8 bytes "TORBOX00"
   1 byte: header len (H)
   H bytes: header, denoting secret key algorithm.
   16 bytes: IV
   Round up to multiple of 128 bytes, then encrypt:
      4 bytes: data len
      data
      zeros
   32 bytes: HMAC-SHA256 of all previous bytes.
*/

#define MAX_OVERHEAD (S2K_MAXLEN + 8 + 1 + 32 + CIPHER_IV_LEN)

/**
 * Make an authenticated passphrase-encrypted blob to encode the
 * <b>input_len</b> bytes in <b>input</b> using the passphrase
 * <b>secret</b> of <b>secret_len</b> bytes.  Allocate a new chunk of memory
 * to hold the encrypted data, and store a pointer to that memory in
 * *<b>out</b>, and its size in <b>outlen_out</b>.  Use <b>s2k_flags</b> as an
 * argument to the passphrase-hashing function.
 */
int
crypto_pwbox(uint8_t **out, size_t *outlen_out,
             const uint8_t *input, size_t input_len,
             const char *secret, size_t secret_len,
             unsigned s2k_flags)
{
  uint8_t *result = NULL, *encrypted_portion;
  size_t encrypted_len = 128 * CEIL_DIV(input_len+4, 128);
  ssize_t result_len;
  int spec_len;
  uint8_t keys[CIPHER_KEY_LEN + DIGEST256_LEN];
  pwbox_encoded_t *enc = NULL;
  ssize_t enc_len;

  crypto_cipher_t *cipher;
  int rv;

  enc = pwbox_encoded_new();

  pwbox_encoded_setlen_skey_header(enc, S2K_MAXLEN);

  spec_len = secret_to_key_make_specifier(
                                      pwbox_encoded_getarray_skey_header(enc),
                                      S2K_MAXLEN,
                                      s2k_flags);
  if (spec_len < 0 || spec_len > S2K_MAXLEN)
    goto err;
  pwbox_encoded_setlen_skey_header(enc, spec_len);
  enc->header_len = spec_len;

  crypto_rand((char*)enc->iv, sizeof(enc->iv));

  pwbox_encoded_setlen_data(enc, encrypted_len);
  encrypted_portion = pwbox_encoded_getarray_data(enc);

  set_uint32(encrypted_portion, htonl((uint32_t)input_len));
  memcpy(encrypted_portion+4, input, input_len);

  /* Now that all the data is in position, derive some keys, encrypt, and
   * digest */
  if (secret_to_key_derivekey(keys, sizeof(keys),
                              pwbox_encoded_getarray_skey_header(enc),
                              spec_len,
                              secret, secret_len) < 0)
    goto err;

  cipher = crypto_cipher_new_with_iv((char*)keys, (char*)enc->iv);
  crypto_cipher_crypt_inplace(cipher, (char*)encrypted_portion, encrypted_len);
  crypto_cipher_free(cipher);

  result_len = pwbox_encoded_encoded_len(enc);
  if (result_len < 0)
    goto err;
  result = tor_malloc(result_len);
  enc_len = pwbox_encoded_encode(result, result_len, enc);
  if (enc_len < 0)
    goto err;
  tor_assert(enc_len == result_len);

  crypto_hmac_sha256((char*) result + result_len - 32,
                     (const char*)keys + CIPHER_KEY_LEN,
                     DIGEST256_LEN,
                     (const char*)result,
                     result_len - 32);

  *out = result;
  *outlen_out = result_len;
  rv = 0;
  goto out;

 err:
  tor_free(result);
  rv = -1;

 out:
  pwbox_encoded_free(enc);
  memwipe(keys, 0, sizeof(keys));
  return rv;
}

/**
 * Try to decrypt the passphrase-encrypted blob of <b>input_len</b> bytes in
 * <b>input</b> using the passphrase <b>secret</b> of <b>secret_len</b> bytes.
 * On success, return 0 and allocate a new chunk of memory to hold the
 * decrypted data, and store a pointer to that memory in *<b>out</b>, and its
 * size in <b>outlen_out</b>.  On failure, return UNPWBOX_BAD_SECRET if
 * the passphrase might have been wrong, and UNPWBOX_CORRUPT if the object is
 * definitely corrupt.
 */
int
crypto_unpwbox(uint8_t **out, size_t *outlen_out,
               const uint8_t *inp, size_t input_len,
               const char *secret, size_t secret_len)
{
  uint8_t *result = NULL;
  const uint8_t *encrypted;
  uint8_t keys[CIPHER_KEY_LEN + DIGEST256_LEN];
  uint8_t hmac[DIGEST256_LEN];
  uint32_t result_len;
  size_t encrypted_len;
  crypto_cipher_t *cipher = NULL;
  int rv = UNPWBOX_CORRUPTED;
  ssize_t got_len;

  pwbox_encoded_t *enc = NULL;

  got_len = pwbox_encoded_parse(&enc, inp, input_len);
  if (got_len < 0 || (size_t)got_len != input_len)
    goto err;

  /* Now derive the keys and check the hmac. */
  if (secret_to_key_derivekey(keys, sizeof(keys),
                              pwbox_encoded_getarray_skey_header(enc),
                              pwbox_encoded_getlen_skey_header(enc),
                              secret, secret_len) < 0)
    goto err;

  crypto_hmac_sha256((char *)hmac,
                     (const char*)keys + CIPHER_KEY_LEN, DIGEST256_LEN,
                     (const char*)inp, input_len - DIGEST256_LEN);

  if (tor_memneq(hmac, enc->hmac, DIGEST256_LEN)) {
    rv = UNPWBOX_BAD_SECRET;
    goto err;
  }

  /* How long is the plaintext? */
  encrypted = pwbox_encoded_getarray_data(enc);
  encrypted_len = pwbox_encoded_getlen_data(enc);
  if (encrypted_len < 4)
    goto err;

  cipher = crypto_cipher_new_with_iv((char*)keys, (char*)enc->iv);
  crypto_cipher_decrypt(cipher, (char*)&result_len, (char*)encrypted, 4);
  result_len = ntohl(result_len);
  if (encrypted_len < result_len + 4)
    goto err;

  /* Allocate a buffer and decrypt */
  result = tor_malloc_zero(result_len);
  crypto_cipher_decrypt(cipher, (char*)result, (char*)encrypted+4, result_len);

  *out = result;
  *outlen_out = result_len;

  rv = UNPWBOX_OKAY;
  goto out;

 err:
  tor_free(result);

 out:
  crypto_cipher_free(cipher);
  pwbox_encoded_free(enc);
  memwipe(keys, 0, sizeof(keys));
  return rv;
}

