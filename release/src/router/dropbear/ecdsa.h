#ifndef DROPBEAR_ECDSA_H_
#define DROPBEAR_ECDSA_H_

#include "includes.h"
#include "buffer.h"
#include "signkey.h"

#ifdef DROPBEAR_ECDSA

/* Prefer the larger size - it's fast anyway */
#if defined(DROPBEAR_ECC_521)
#define ECDSA_DEFAULT_SIZE 521
#elif defined(DROPBEAR_ECC_384)
#define ECDSA_DEFAULT_SIZE 384
#elif defined(DROPBEAR_ECC_256)
#define ECDSA_DEFAULT_SIZE 256
#else
#define ECDSA_DEFAULT_SIZE 0
#endif

ecc_key *gen_ecdsa_priv_key(unsigned int bit_size);
ecc_key *buf_get_ecdsa_pub_key(buffer* buf);
ecc_key *buf_get_ecdsa_priv_key(buffer *buf);
void buf_put_ecdsa_pub_key(buffer *buf, ecc_key *key);
void buf_put_ecdsa_priv_key(buffer *buf, ecc_key *key);
enum signkey_type ecdsa_signkey_type(ecc_key * key);

void buf_put_ecdsa_sign(buffer *buf, ecc_key *key, buffer *data_buf);
int buf_ecdsa_verify(buffer *buf, ecc_key *key, buffer *data_buf);
/* Returns 1 on success */
int signkey_is_ecdsa(enum signkey_type type);

#endif

#endif /* DROPBEAR_ECDSA_H_ */
