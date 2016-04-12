/* Added for Tor. */
#include <openssl/sha.h>

/* Set 'out' to the 512-bit SHA512 hash of the 'len'-byte string in 'inp' */
#define crypto_hash_sha512(out, inp, len) \
  SHA512((inp), (len), (out))

/* Set 'out' to the 512-bit SHA512 hash of the 'len1'-byte string in 'inp1',
 * concatenated with the 'len2'-byte string in 'inp2'. */
#define crypto_hash_sha512_2(out, inp1, len1, inp2, len2)               \
  do {                                                                  \
      SHA512_CTX sha_ctx_;                                              \
      SHA512_Init(&sha_ctx_);                                           \
      SHA512_Update(&sha_ctx_, (inp1), (len1));                         \
      SHA512_Update(&sha_ctx_, (inp2), (len2));                         \
      SHA512_Final((out), &sha_ctx_);                                   \
 } while(0)

/* Set 'out' to the 512-bit SHA512 hash of the 'len1'-byte string in 'inp1',
 * concatenated with the 'len2'-byte string in 'inp2', concatenated with
 * the 'len3'-byte string in 'len3'. */
#define crypto_hash_sha512_3(out, inp1, len1, inp2, len2, inp3, len3)   \
  do {                                                                  \
      SHA512_CTX sha_ctx_;                                              \
      SHA512_Init(&sha_ctx_);                                           \
      SHA512_Update(&sha_ctx_, (inp1), (len1));                         \
      SHA512_Update(&sha_ctx_, (inp2), (len2));                         \
      SHA512_Update(&sha_ctx_, (inp3), (len3));                         \
      SHA512_Final((out), &sha_ctx_);                                   \
 } while(0)
