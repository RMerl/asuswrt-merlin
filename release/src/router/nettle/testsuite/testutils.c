/* testutils.c */

#include "testutils.h"

#include "base16.h"
#include "cbc.h"
#include "ctr.h"
#include "knuth-lfib.h"
#include "macros.h"
#include "nettle-internal.h"

#include <assert.h>
#include <ctype.h>

void
die(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  abort ();
}

void *
xalloc(size_t size)
{
  void *p = malloc(size);
  if (size && !p)
    {
      fprintf(stderr, "Virtual memory exhausted.\n");
      abort();
    }

  return p;
}

static struct tstring *tstring_first = NULL;

struct tstring *
tstring_alloc (size_t length)
{
  struct tstring *s = xalloc(sizeof(struct tstring) + length);
  s->length = length;
  s->next = tstring_first;
  /* NUL-terminate, for convenience. */
  s->data[length] = '\0';
  tstring_first = s;
  return s;
}

void
tstring_clear(void)
{
  while (tstring_first)
    {
      struct tstring *s = tstring_first;
      tstring_first = s->next;
      free(s);
    }
}

struct tstring *
tstring_data(size_t length, const char *data)
{
  struct tstring *s = tstring_alloc (length);
  memcpy (s->data, data, length);
  return s;
}

struct tstring *
tstring_hex(const char *hex)
{
  struct base16_decode_ctx ctx;
  struct tstring *s;
  size_t length = strlen(hex);

  s = tstring_alloc(BASE16_DECODE_LENGTH (length));
  base16_decode_init (&ctx);
  ASSERT (base16_decode_update (&ctx, &s->length, s->data,
				length, hex));
  ASSERT (base16_decode_final (&ctx));

  return s;
}

void
tstring_print_hex(const struct tstring *s)
{
  print_hex (s->length, s->data);
}

void
print_hex(size_t length, const uint8_t *data)
{
  size_t i;
  
  for (i = 0; i < length; i++)
    {
      switch (i % 16)
	{
	default:
	  break;
	case 0:
	  printf("\n");
	  break;
	case 8:
	  printf(" ");
	  break;
	}
      printf("%02x", data[i]);
    }
  printf("\n");
}

int verbose = 0;

int
main(int argc, char **argv)
{
  if (argc > 1)
    {
      if (argc == 2 && !strcmp(argv[1], "-v"))
	verbose = 1;
      else
	{
	  fprintf(stderr, "Invalid argument `%s', only accepted option is `-v'.\n",
		  argv[1]);
	  return 1;
	}
    }

  test_main();

  tstring_clear();
  return EXIT_SUCCESS;
}

void
test_cipher(const struct nettle_cipher *cipher,
	    const struct tstring *key,
	    const struct tstring *cleartext,
	    const struct tstring *ciphertext)
{
  void *ctx = xalloc(cipher->context_size);
  uint8_t *data = xalloc(cleartext->length);
  size_t length;
  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  ASSERT (key->length == cipher->key_size);
  cipher->set_encrypt_key(ctx, key->data);
  cipher->encrypt(ctx, length, data, cleartext->data);

  if (!MEMEQ(length, data, ciphertext->data))
    {
      fprintf(stderr, "Encrypt failed:\nInput:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }
  cipher->set_decrypt_key(ctx, key->data);
  cipher->decrypt(ctx, length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "Decrypt failed:\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }

  free(ctx);
  free(data);
}

void
test_cipher_cbc(const struct nettle_cipher *cipher,
		const struct tstring *key,
		const struct tstring *cleartext,
		const struct tstring *ciphertext,
		const struct tstring *iiv)
{
  void *ctx = xalloc(cipher->context_size);
  uint8_t *data;
  uint8_t *iv = xalloc(cipher->block_size);
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  ASSERT (key->length == cipher->key_size);
  ASSERT (iiv->length == cipher->block_size);

  data = xalloc(length);  
  cipher->set_encrypt_key(ctx, key->data);
  memcpy(iv, iiv->data, cipher->block_size);

  cbc_encrypt(ctx, cipher->encrypt,
	      cipher->block_size, iv,
	      length, data, cleartext->data);

  if (!MEMEQ(length, data, ciphertext->data))
    {
      fprintf(stderr, "CBC encrypt failed:\nInput:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }
  cipher->set_decrypt_key(ctx, key->data);
  memcpy(iv, iiv->data, cipher->block_size);

  cbc_decrypt(ctx, cipher->decrypt,
	      cipher->block_size, iv,
	      length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "CBC decrypt failed:\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }

  free(ctx);
  free(data);
  free(iv);
}

void
test_cipher_ctr(const struct nettle_cipher *cipher,
		const struct tstring *key,
		const struct tstring *cleartext,
		const struct tstring *ciphertext,
		const struct tstring *ictr)
{
  void *ctx = xalloc(cipher->context_size);
  uint8_t *data;
  uint8_t *ctr = xalloc(cipher->block_size);
  uint8_t *octr = xalloc(cipher->block_size);
  size_t length, nblocks;
  unsigned low;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  ASSERT (key->length == cipher->key_size);
  ASSERT (ictr->length == cipher->block_size);

  /* Compute expected counter value after the operation. */
  nblocks = (length + cipher->block_size - 1) / cipher->block_size;
  ASSERT (nblocks < 0x100);

  memcpy (octr, ictr->data, cipher->block_size - 1);
  low = ictr->data[cipher->block_size - 1] + nblocks;
  octr[cipher->block_size - 1] = low;

  if (low >= 0x100)
    INCREMENT (cipher->block_size - 1, octr);

  data = xalloc(length);  

  cipher->set_encrypt_key(ctx, key->data);
  memcpy(ctr, ictr->data, cipher->block_size);

  ctr_crypt(ctx, cipher->encrypt,
	    cipher->block_size, ctr,
	    length, data, cleartext->data);

  if (!MEMEQ(length, data, ciphertext->data))
    {
      fprintf(stderr, "CTR encrypt failed:\nInput:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }

  ASSERT (MEMEQ (cipher->block_size, ctr, octr));

  memcpy(ctr, ictr->data, cipher->block_size);

  ctr_crypt(ctx, cipher->encrypt,
	    cipher->block_size, ctr,
	    length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "CTR decrypt failed:\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }

  ASSERT (MEMEQ (cipher->block_size, ctr, octr));

  free(ctx);
  free(data);
  free(octr);
  free(ctr);
}

#if 0
void
test_cipher_stream(const struct nettle_cipher *cipher,
		   const struct tstring *key,
		   const struct tstring *cleartext,
		   const struct tstring *ciphertext)
{
  size_t block;
  
  void *ctx = xalloc(cipher->context_size);
  uint8_t *data;
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  data = xalloc(length + 1);

  for (block = 1; block <= length; block++)
    {
      size_t i;

      memset(data, 0x17, length + 1);
      cipher->set_encrypt_key(ctx, key->length, key->data);

      for (i = 0; i + block < length; i += block)
	{
	  cipher->encrypt(ctx, block, data + i, cleartext->data + i);
	  ASSERT (data[i + block] == 0x17);
	}

      cipher->encrypt(ctx, length - i, data + i, cleartext->data + i);
      ASSERT (data[length] == 0x17);
      
      if (!MEMEQ(length, data, ciphertext->data))
	{
	  fprintf(stderr, "Encrypt failed, block size %lu\nInput:",
		  (unsigned long) block);
	  tstring_print_hex(cleartext);
	  fprintf(stderr, "\nOutput: ");
	  print_hex(length, data);
	  fprintf(stderr, "\nExpected:");
	  tstring_print_hex(ciphertext);
	  fprintf(stderr, "\n");
	  FAIL();	    
	}
    }
  
  cipher->set_decrypt_key(ctx, key->length, key->data);
  cipher->decrypt(ctx, length, data, data);

  ASSERT (data[length] == 0x17);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "Decrypt failed\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();	    
    }

  free(ctx);
  free(data);
}
#endif

void
test_aead(const struct nettle_aead *aead,
	  nettle_hash_update_func *set_nonce,
	  const struct tstring *key,
	  const struct tstring *authtext,
	  const struct tstring *cleartext,
	  const struct tstring *ciphertext,
	  const struct tstring *nonce,
	  const struct tstring *digest)
{
  void *ctx = xalloc(aead->context_size);
  uint8_t *data;
  uint8_t *buffer = xalloc(aead->digest_size);
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  ASSERT (key->length == aead->key_size);
  ASSERT (digest->length <= aead->digest_size);

  data = xalloc(length);
  
  /* encryption */
  memset(buffer, 0, aead->digest_size);
  aead->set_encrypt_key(ctx, key->data);

  if (nonce->length != aead->nonce_size)
    {
      ASSERT (set_nonce);
      set_nonce (ctx, nonce->length, nonce->data);
    }
  else
    aead->set_nonce(ctx, nonce->data);

  if (authtext->length)
    aead->update(ctx, authtext->length, authtext->data);
    
  if (length)
    aead->encrypt(ctx, length, data, cleartext->data);

  aead->digest(ctx, digest->length, buffer);

  ASSERT(MEMEQ(length, data, ciphertext->data));
  ASSERT(MEMEQ(digest->length, buffer, digest->data));

  /* decryption */
  memset(buffer, 0, aead->digest_size);

  aead->set_decrypt_key(ctx, key->data);

  if (nonce->length != aead->nonce_size)
    {
      ASSERT (set_nonce);
      set_nonce (ctx, nonce->length, nonce->data);
    }
  else
    aead->set_nonce(ctx, nonce->data);

  if (authtext->length)
    aead->update(ctx, authtext->length, authtext->data);
    
  if (length)
    aead->decrypt(ctx, length, data, data);

  aead->digest(ctx, digest->length, buffer);

  ASSERT(MEMEQ(length, data, cleartext->data));
  ASSERT(MEMEQ(digest->length, buffer, digest->data));

  free(ctx);
  free(data);
  free(buffer);
}

void
test_hash(const struct nettle_hash *hash,
	  const struct tstring *msg,
	  const struct tstring *digest)
{
  void *ctx = xalloc(hash->context_size);
  uint8_t *buffer = xalloc(hash->digest_size);
  uint8_t *input;
  unsigned offset;

  ASSERT (digest->length == hash->digest_size);

  hash->init(ctx);
  hash->update(ctx, msg->length, msg->data);
  hash->digest(ctx, hash->digest_size, buffer);

  if (MEMEQ(hash->digest_size, digest->data, buffer) == 0)
    {
      fprintf(stdout, "\nGot:\n");
      print_hex(hash->digest_size, buffer);
      fprintf(stdout, "\nExpected:\n");
      print_hex(hash->digest_size, digest->data);
      abort();
    }

  memset(buffer, 0, hash->digest_size);

  hash->update(ctx, msg->length, msg->data);
  hash->digest(ctx, hash->digest_size - 1, buffer);

  ASSERT(MEMEQ(hash->digest_size - 1, digest->data, buffer));

  ASSERT(buffer[hash->digest_size - 1] == 0);

  input = xalloc (msg->length + 16);
  for (offset = 0; offset < 16; offset++)
    {
      memset (input, 0, msg->length + 16);
      memcpy (input + offset, msg->data, msg->length);
      hash->update (ctx, msg->length, input + offset);
      hash->digest (ctx, hash->digest_size, buffer);
      if (MEMEQ(hash->digest_size, digest->data, buffer) == 0)
	{
	  fprintf(stdout, "hash input address: %p\nGot:\n", input + offset);
	  print_hex(hash->digest_size, buffer);
	  fprintf(stdout, "\nExpected:\n");
	  print_hex(hash->digest_size, digest->data);
	  abort();
	}      
    }
  free(ctx);
  free(buffer);
  free(input);
}

void
test_hash_large(const struct nettle_hash *hash,
		size_t count, size_t length,
		uint8_t c,
		const struct tstring *digest)
{
  void *ctx = xalloc(hash->context_size);
  uint8_t *buffer = xalloc(hash->digest_size);
  uint8_t *data = xalloc(length);
  size_t i;

  ASSERT (digest->length == hash->digest_size);

  memset(data, c, length);

  hash->init(ctx);
  for (i = 0; i < count; i++)
    {
      hash->update(ctx, length, data);
      if (i % (count / 50) == 0)
	fprintf (stderr, ".");
    }
  fprintf (stderr, "\n");
  
  hash->digest(ctx, hash->digest_size, buffer);

  print_hex(hash->digest_size, buffer);

  ASSERT (MEMEQ(hash->digest_size, digest->data, buffer));

  free(ctx);
  free(buffer);
  free(data);
}

void
test_armor(const struct nettle_armor *armor,
           size_t data_length,
           const uint8_t *data,
           const uint8_t *ascii)
{
  size_t ascii_length = strlen(ascii);
  uint8_t *buffer = xalloc(1 + ascii_length);
  uint8_t *check = xalloc(1 + armor->decode_length(ascii_length));
  void *encode = xalloc(armor->encode_context_size);
  void *decode = xalloc(armor->decode_context_size);
  size_t done;

  ASSERT(ascii_length
	 <= (armor->encode_length(data_length) + armor->encode_final_length));
  ASSERT(data_length <= armor->decode_length(ascii_length));
  
  memset(buffer, 0x33, 1 + ascii_length);
  memset(check, 0x55, 1 + data_length);

  armor->encode_init(encode);
  
  done = armor->encode_update(encode, buffer, data_length, data);
  done += armor->encode_final(encode, buffer + done);
  ASSERT(done == ascii_length);

  ASSERT (MEMEQ(ascii_length, buffer, ascii));
  ASSERT (0x33 == buffer[strlen(ascii)]);

  armor->decode_init(decode);
  done = armor->decode_length(ascii_length);

  ASSERT(armor->decode_update(decode, &done, check, ascii_length, buffer));
  ASSERT(done == data_length);
  ASSERT(armor->decode_final(decode));
  
  ASSERT (MEMEQ(data_length, check, data));
  ASSERT (0x55 == check[data_length]);

  free(buffer);
  free(check);
  free(encode);
  free(decode);
}

#if WITH_HOGWEED

#ifndef mpz_combit
/* Missing in older gmp */
static void
mpz_combit (mpz_t x, unsigned long int bit)
{
  if (mpz_tstbit(x, bit))
    mpz_clrbit(x, bit);
  else
    mpz_setbit(x, bit);
}
#endif

#ifndef mpn_zero_p
int
mpn_zero_p (mp_srcptr ap, mp_size_t n)
{
  while (--n >= 0)
    {
      if (ap[n] != 0)
	return 0;
    }
  return 1;
}
#endif

void
mpn_out_str (FILE *f, int base, const mp_limb_t *xp, mp_size_t xn)
{
  mpz_t x;
  mpz_out_str (f, base, mpz_roinit_n (x, xp, xn));
}

#if NETTLE_USE_MINI_GMP
void
gmp_randinit_default (struct knuth_lfib_ctx *ctx)
{
  knuth_lfib_init (ctx, 17);
}
void
mpz_urandomb (mpz_t r, struct knuth_lfib_ctx *ctx, mp_bitcnt_t bits)
{
  size_t bytes = (bits+7)/8;
  uint8_t *buf = xalloc (bytes);

  knuth_lfib_random (ctx, bytes, buf);
  buf[bytes-1] &= 0xff >> (8*bytes - bits);
  nettle_mpz_set_str_256_u (r, bytes, buf);
  free (buf);
}
#endif /* NETTLE_USE_MINI_GMP */

mp_limb_t *
xalloc_limbs (mp_size_t n)
{
  return xalloc (n * sizeof (mp_limb_t));
}

#define SIGN(key, hash, msg, signature) do {		\
  hash##_update(&hash, LDATA(msg));		\
  ASSERT(rsa_##hash##_sign(key, &hash, signature));	\
} while(0)

#define VERIFY(key, hash, msg, signature) (	\
  hash##_update(&hash, LDATA(msg)),		\
  rsa_##hash##_verify(key, &hash, signature)	\
)

void
test_rsa_set_key_1(struct rsa_public_key *pub,
		   struct rsa_private_key *key)
{
  /* Initialize key pair for test programs */
  /* 1000-bit key, generated by
   *
   *   lsh-keygen -a rsa -l 1000 -f advanced-hex
   *
   * (private-key (rsa-pkcs1 
   *        (n #69abd505285af665 36ddc7c8f027e6f0 ed435d6748b16088
   *            4fd60842b3a8d7fb bd8a3c98f0cc50ae 4f6a9f7dd73122cc
   *            ec8afa3f77134406 f53721973115fc2d 8cfbba23b145f28d
   *            84f81d3b6ae8ce1e 2850580c026e809b cfbb52566ea3a3b3
   *            df7edf52971872a7 e35c1451b8636d22 279a8fb299368238
   *            e545fbb4cf#)
   *        (e #0db2ad57#)
   *        (d #3240a56f4cd0dcc2 4a413eb4ea545259 5c83d771a1c2ba7b
   *            ec47c5b43eb4b374 09bd2aa1e236dd86 481eb1768811412f
   *            f8d91be3545912af b55c014cb55ceac6 54216af3b85d5c4f
   *            4a32894e3b5dfcde 5b2875aa4dc8d9a8 6afd0ca92ef50d35
   *            bd09f1c47efb4c8d c631e07698d362aa 4a83fd304e66d6c5
   *            468863c307#)
   *        (p #0a66399919be4b4d e5a78c5ea5c85bf9 aba8c013cb4a8732
   *            14557a12bd67711e bb4073fd39ad9a86 f4e80253ad809e5b
   *            f2fad3bc37f6f013 273c9552c9f489#)
   *        (q #0a294f069f118625 f5eae2538db9338c 776a298eae953329
   *            9fd1eed4eba04e82 b2593bc98ba8db27 de034da7daaea795
   *            2d55b07b5f9a5875 d1ca5f6dcab897#)
   *        (a #011b6c48eb592eee e85d1bb35cfb6e07 344ea0b5e5f03a28
   *            5b405396cbc78c5c 868e961db160ba8d 4b984250930cf79a
   *            1bf8a9f28963de53 128aa7d690eb87#)
   *        (b #0409ecf3d2557c88 214f1af5e1f17853 d8b2d63782fa5628
   *            60cf579b0833b7ff 5c0529f2a97c6452 2fa1a8878a9635ab
   *            ce56debf431bdec2 70b308fa5bf387#)
   *        (c #04e103ee925cb5e6 6653949fa5e1a462 c9e65e1adcd60058
   *            e2df9607cee95fa8 daec7a389a7d9afc 8dd21fef9d83805a
   *            40d46f49676a2f6b 2926f70c572c00#)))
   */
  
  mpz_set_str(pub->n,
	      "69abd505285af665" "36ddc7c8f027e6f0" "ed435d6748b16088"
	      "4fd60842b3a8d7fb" "bd8a3c98f0cc50ae" "4f6a9f7dd73122cc"
	      "ec8afa3f77134406" "f53721973115fc2d" "8cfbba23b145f28d"
	      "84f81d3b6ae8ce1e" "2850580c026e809b" "cfbb52566ea3a3b3"
	      "df7edf52971872a7" "e35c1451b8636d22" "279a8fb299368238"
	      "e545fbb4cf", 16);
  mpz_set_str(pub->e, "0db2ad57", 16);

  ASSERT (rsa_public_key_prepare(pub));
  
  /* d is not used */
#if 0  
  mpz_set_str(key->d,
	      "3240a56f4cd0dcc2" "4a413eb4ea545259" "5c83d771a1c2ba7b"
	      "ec47c5b43eb4b374" "09bd2aa1e236dd86" "481eb1768811412f"
	      "f8d91be3545912af" "b55c014cb55ceac6" "54216af3b85d5c4f"
	      "4a32894e3b5dfcde" "5b2875aa4dc8d9a8" "6afd0ca92ef50d35"
	      "bd09f1c47efb4c8d" "c631e07698d362aa" "4a83fd304e66d6c5"
	      "468863c307", 16);
#endif
  
  mpz_set_str(key->p,
	      "0a66399919be4b4d" "e5a78c5ea5c85bf9" "aba8c013cb4a8732"
	      "14557a12bd67711e" "bb4073fd39ad9a86" "f4e80253ad809e5b"
	      "f2fad3bc37f6f013" "273c9552c9f489", 16);

  mpz_set_str(key->q,
	      "0a294f069f118625" "f5eae2538db9338c" "776a298eae953329"
	      "9fd1eed4eba04e82" "b2593bc98ba8db27" "de034da7daaea795"
	      "2d55b07b5f9a5875" "d1ca5f6dcab897", 16);
  
  mpz_set_str(key->a,
	      "011b6c48eb592eee" "e85d1bb35cfb6e07" "344ea0b5e5f03a28"
	      "5b405396cbc78c5c" "868e961db160ba8d" "4b984250930cf79a"
	      "1bf8a9f28963de53" "128aa7d690eb87", 16);
  
  mpz_set_str(key->b,
	      "0409ecf3d2557c88" "214f1af5e1f17853" "d8b2d63782fa5628"
	      "60cf579b0833b7ff" "5c0529f2a97c6452" "2fa1a8878a9635ab"
	      "ce56debf431bdec2" "70b308fa5bf387", 16);
  
  mpz_set_str(key->c,
	      "04e103ee925cb5e6" "6653949fa5e1a462" "c9e65e1adcd60058"
	      "e2df9607cee95fa8" "daec7a389a7d9afc" "8dd21fef9d83805a"
	      "40d46f49676a2f6b" "2926f70c572c00", 16);

  ASSERT (rsa_private_key_prepare(key));
  ASSERT (pub->size == key->size);
}

void
test_rsa_md5(struct rsa_public_key *pub,
	     struct rsa_private_key *key,
	     mpz_t expected)
{
  struct md5_ctx md5;
  mpz_t signature;

  md5_init(&md5);
  mpz_init(signature);
  
  SIGN(key, md5, "The magic words are squeamish ossifrage", signature);

  if (verbose)
    {
      fprintf(stderr, "rsa-md5 signature: ");
      mpz_out_str(stderr, 16, signature);
      fprintf(stderr, "\n");
    }

  ASSERT (mpz_cmp(signature, expected) == 0);
  
  /* Try bad data */
  ASSERT (!VERIFY(pub, md5,
		  "The magick words are squeamish ossifrage", signature));

  /* Try correct data */
  ASSERT (VERIFY(pub, md5,
		 "The magic words are squeamish ossifrage", signature));

  /* Try bad signature */
  mpz_combit(signature, 17);
  ASSERT (!VERIFY(pub, md5,
		  "The magic words are squeamish ossifrage", signature));

  mpz_clear(signature);
}

void
test_rsa_sha1(struct rsa_public_key *pub,
	      struct rsa_private_key *key,
	      mpz_t expected)
{
  struct sha1_ctx sha1;
  mpz_t signature;

  sha1_init(&sha1);
  mpz_init(signature);

  SIGN(key, sha1, "The magic words are squeamish ossifrage", signature);

  if (verbose)
    {
      fprintf(stderr, "rsa-sha1 signature: ");
      mpz_out_str(stderr, 16, signature);
      fprintf(stderr, "\n");
    }

  ASSERT (mpz_cmp(signature, expected) == 0);
  
  /* Try bad data */
  ASSERT (!VERIFY(pub, sha1,
		  "The magick words are squeamish ossifrage", signature));

  /* Try correct data */
  ASSERT (VERIFY(pub, sha1,
		 "The magic words are squeamish ossifrage", signature));

  /* Try bad signature */
  mpz_combit(signature, 17);
  ASSERT (!VERIFY(pub, sha1,
		  "The magic words are squeamish ossifrage", signature));

  mpz_clear(signature);
}

void
test_rsa_sha256(struct rsa_public_key *pub,
		struct rsa_private_key *key,
		mpz_t expected)
{
  struct sha256_ctx sha256;
  mpz_t signature;

  sha256_init(&sha256);
  mpz_init(signature);

  SIGN(key, sha256, "The magic words are squeamish ossifrage", signature);

  if (verbose)
    {
      fprintf(stderr, "rsa-sha256 signature: ");
      mpz_out_str(stderr, 16, signature);
      fprintf(stderr, "\n");
    }

  ASSERT (mpz_cmp(signature, expected) == 0);
  
  /* Try bad data */
  ASSERT (!VERIFY(pub, sha256,
		  "The magick words are squeamish ossifrage", signature));

  /* Try correct data */
  ASSERT (VERIFY(pub, sha256,
		 "The magic words are squeamish ossifrage", signature));

  /* Try bad signature */
  mpz_combit(signature, 17);
  ASSERT (!VERIFY(pub, sha256,
		  "The magic words are squeamish ossifrage", signature));

  mpz_clear(signature);
}

void
test_rsa_sha512(struct rsa_public_key *pub,
		struct rsa_private_key *key,
		mpz_t expected)
{
  struct sha512_ctx sha512;
  mpz_t signature;

  sha512_init(&sha512);
  mpz_init(signature);

  SIGN(key, sha512, "The magic words are squeamish ossifrage", signature);

  if (verbose)
    {
      fprintf(stderr, "rsa-sha512 signature: ");
      mpz_out_str(stderr, 16, signature);
      fprintf(stderr, "\n");
    }

  ASSERT (mpz_cmp(signature, expected) == 0);
  
  /* Try bad data */
  ASSERT (!VERIFY(pub, sha512,
		  "The magick words are squeamish ossifrage", signature));

  /* Try correct data */
  ASSERT (VERIFY(pub, sha512,
		 "The magic words are squeamish ossifrage", signature));

  /* Try bad signature */
  mpz_combit(signature, 17);
  ASSERT (!VERIFY(pub, sha512,
		  "The magic words are squeamish ossifrage", signature));

  mpz_clear(signature);
}

#undef SIGN
#undef VERIFY

void
test_rsa_key(struct rsa_public_key *pub,
	     struct rsa_private_key *key)
{
  mpz_t tmp;
  mpz_t phi;
  
  mpz_init(tmp); mpz_init(phi);
  
  if (verbose)
    {
      /* FIXME: Use gmp_printf */
      fprintf(stderr, "Public key: n=");
      mpz_out_str(stderr, 16, pub->n);
      fprintf(stderr, "\n    e=");
      mpz_out_str(stderr, 16, pub->e);

      fprintf(stderr, "\n\nPrivate key: d=");
      mpz_out_str(stderr, 16, key->d);
      fprintf(stderr, "\n    p=");
      mpz_out_str(stderr, 16, key->p);
      fprintf(stderr, "\n    q=");
      mpz_out_str(stderr, 16, key->q);
      fprintf(stderr, "\n    a=");
      mpz_out_str(stderr, 16, key->a);
      fprintf(stderr, "\n    b=");
      mpz_out_str(stderr, 16, key->b);
      fprintf(stderr, "\n    c=");
      mpz_out_str(stderr, 16, key->c);
      fprintf(stderr, "\n\n");
    }

  /* Check n = p q */
  mpz_mul(tmp, key->p, key->q);
  ASSERT (mpz_cmp(tmp, pub->n)== 0);

  /* Check c q = 1 mod p */
  mpz_mul(tmp, key->c, key->q);
  mpz_fdiv_r(tmp, tmp, key->p);
  ASSERT (mpz_cmp_ui(tmp, 1) == 0);

  /* Check ed = 1 (mod phi) */
  mpz_sub_ui(phi, key->p, 1);
  mpz_sub_ui(tmp, key->q, 1);

  mpz_mul(phi, phi, tmp);

  mpz_mul(tmp, pub->e, key->d);
  mpz_fdiv_r(tmp, tmp, phi);
  ASSERT (mpz_cmp_ui(tmp, 1) == 0);

  /* Check a e = 1 (mod (p-1) ) */
  mpz_sub_ui(phi, key->p, 1);
  mpz_mul(tmp, pub->e, key->a);
  mpz_fdiv_r(tmp, tmp, phi);
  ASSERT (mpz_cmp_ui(tmp, 1) == 0);
  
  /* Check b e = 1 (mod (q-1) ) */
  mpz_sub_ui(phi, key->q, 1);
  mpz_mul(tmp, pub->e, key->b);
  mpz_fdiv_r(tmp, tmp, phi);
  ASSERT (mpz_cmp_ui(tmp, 1) == 0);
  
  mpz_clear(tmp); mpz_clear(phi);
}

/* Requires that the context is named like the hash algorithm. */
#define DSA_VERIFY(key, hash, msg, signature)	\
  (hash##_update(&hash, LDATA(msg)),		\
   dsa_##hash##_verify(key, &hash, signature))

void
test_dsa160(const struct dsa_public_key *pub,
	    const struct dsa_private_key *key,
	    const struct dsa_signature *expected)
{
  struct sha1_ctx sha1;
  struct dsa_signature signature;
  struct knuth_lfib_ctx lfib;
  
  sha1_init(&sha1);
  dsa_signature_init(&signature);
  knuth_lfib_init(&lfib, 1111);
  
  sha1_update(&sha1, LDATA("The magic words are squeamish ossifrage"));
  ASSERT (dsa_sha1_sign(pub, key,
			&lfib, (nettle_random_func *) knuth_lfib_random,
			&sha1, &signature));

  if (verbose)
    {
      fprintf(stderr, "dsa160 signature: ");
      mpz_out_str(stderr, 16, signature.r);
      fprintf(stderr, ", ");
      mpz_out_str(stderr, 16, signature.s);
      fprintf(stderr, "\n");
    }

  if (expected)
    ASSERT (mpz_cmp (signature.r, expected->r) == 0
	    && mpz_cmp (signature.s, expected->s) == 0);
  
  /* Try bad data */
  ASSERT (!DSA_VERIFY(pub, sha1,
		      "The magick words are squeamish ossifrage",
		      &signature));

  /* Try correct data */
  ASSERT (DSA_VERIFY(pub, sha1,
		     "The magic words are squeamish ossifrage",
		     &signature));

  /* Try bad signature */
  mpz_combit(signature.r, 17);
  ASSERT (!DSA_VERIFY(pub, sha1,
		      "The magic words are squeamish ossifrage",
		      &signature));

  dsa_signature_clear(&signature);
}

void
test_dsa256(const struct dsa_public_key *pub,
	    const struct dsa_private_key *key,
	    const struct dsa_signature *expected)
{
  struct sha256_ctx sha256;
  struct dsa_signature signature;
  struct knuth_lfib_ctx lfib;
  
  sha256_init(&sha256);
  dsa_signature_init(&signature);
  knuth_lfib_init(&lfib, 1111);
  
  sha256_update(&sha256, LDATA("The magic words are squeamish ossifrage"));
  ASSERT (dsa_sha256_sign(pub, key,
			&lfib, (nettle_random_func *) knuth_lfib_random,
			&sha256, &signature));
  
  if (verbose)
    {
      fprintf(stderr, "dsa256 signature: ");
      mpz_out_str(stderr, 16, signature.r);
      fprintf(stderr, ", ");
      mpz_out_str(stderr, 16, signature.s);
      fprintf(stderr, "\n");
    }

  if (expected)
    ASSERT (mpz_cmp (signature.r, expected->r) == 0
	    && mpz_cmp (signature.s, expected->s) == 0);
  
  /* Try bad data */
  ASSERT (!DSA_VERIFY(pub, sha256,
		      "The magick words are squeamish ossifrage",
		      &signature));

  /* Try correct data */
  ASSERT (DSA_VERIFY(pub, sha256,
		     "The magic words are squeamish ossifrage",
		     &signature));

  /* Try bad signature */
  mpz_combit(signature.r, 17);
  ASSERT (!DSA_VERIFY(pub, sha256,
		      "The magic words are squeamish ossifrage",
		      &signature));

  dsa_signature_clear(&signature);
}

#if 0
void
test_dsa_sign(const struct dsa_public_key *pub,
	      const struct dsa_private_key *key,
	      const struct nettle_hash *hash,
	      const struct dsa_signature *expected)
{
  void *ctx = xalloc (hash->context_size);
  uint8_t *digest = xalloc (hash->digest_size);
  uint8_t *bad_digest = xalloc (hash->digest_size);
  struct dsa_signature signature;
  struct knuth_lfib_ctx lfib;
  
  dsa_signature_init(&signature);
  knuth_lfib_init(&lfib, 1111);

  hash->init(ctx);
  
  hash->update(ctx, LDATA("The magic words are squeamish ossifrage"));
  hash->digest(ctx, hash->digest_size, digest);
  ASSERT (dsa_sign(pub, key,
		   &lfib, (nettle_random_func *) knuth_lfib_random,
		   hash->digest_size, digest, &signature));
  
  if (verbose)
    {
      fprintf(stderr, "dsa-%s signature: ", hash->name);
      mpz_out_str(stderr, 16, signature.r);
      fprintf(stderr, ", ");
      mpz_out_str(stderr, 16, signature.s);
      fprintf(stderr, "\n");
    }

  if (expected)
    ASSERT (mpz_cmp (signature.r, expected->r) == 0
	    && mpz_cmp (signature.s, expected->s) == 0);
  
  /* Try correct data */
  ASSERT (dsa_verify(pub, hash->digest_size, digest,
		     &signature));
  /* Try bad data */
  hash->update(ctx, LDATA("The magick words are squeamish ossifrage"));
  hash->digest(ctx, hash->digest_size, bad_digest);
  
  ASSERT (!dsa_verify(pub, hash->digest_size, bad_digest,
		      &signature));

  /* Try bad signature */
  mpz_combit(signature.r, 17);
  ASSERT (!dsa_verify(pub, hash->digest_size, digest,
		      &signature));

  free (ctx);
  free (digest);
  free (bad_digest);
  dsa_signature_clear(&signature);
}
#endif

void
test_dsa_verify(const struct dsa_params *params,
		const mpz_t pub,
		const struct nettle_hash *hash,
		struct tstring *msg,
		const struct dsa_signature *ref)
{
  void *ctx = xalloc (hash->context_size);
  uint8_t *digest = xalloc (hash->digest_size);
  struct dsa_signature signature;

  dsa_signature_init (&signature);

  hash->init(ctx);
  
  hash->update (ctx, msg->length, msg->data);
  hash->digest (ctx, hash->digest_size, digest);

  mpz_set (signature.r, ref->r);
  mpz_set (signature.s, ref->s);

  ASSERT (dsa_verify (params, pub,
		       hash->digest_size, digest,
		       &signature));

  /* Try bad signature */
  mpz_combit(signature.r, 17);
  ASSERT (!dsa_verify (params, pub,
		       hash->digest_size, digest,
		       &signature));
  
  /* Try bad data */
  digest[hash->digest_size / 2-1] ^= 8;
  ASSERT (!dsa_verify (params, pub,
		       hash->digest_size, digest,
		       ref));

  free (ctx);
  free (digest);
  dsa_signature_clear(&signature);  
}

void
test_dsa_key(const struct dsa_params *params,
	     const mpz_t pub,
	     const mpz_t key,
	     unsigned q_size)
{
  mpz_t t;

  mpz_init(t);

  ASSERT(mpz_sizeinbase(params->q, 2) == q_size);
  ASSERT(mpz_sizeinbase(params->p, 2) >= DSA_SHA1_MIN_P_BITS);
  
  ASSERT(mpz_probab_prime_p(params->p, 10));

  ASSERT(mpz_probab_prime_p(params->q, 10));

  mpz_fdiv_r(t, params->p, params->q);

  ASSERT(0 == mpz_cmp_ui(t, 1));

  ASSERT(mpz_cmp_ui(params->g, 1) > 0);
  
  mpz_powm(t, params->g, params->q, params->p);
  ASSERT(0 == mpz_cmp_ui(t, 1));
  
  mpz_powm(t, params->g, key, params->p);
  ASSERT(0 == mpz_cmp(t, pub));

  mpz_clear(t);
}

const struct ecc_curve * const ecc_curves[] = {
  &nettle_secp_192r1,
  &nettle_secp_224r1,
  &nettle_secp_256r1,
  &nettle_secp_384r1,
  &nettle_secp_521r1,
  &_nettle_curve25519,
  NULL
};

static int
test_mpn (const char *ref, const mp_limb_t *xp, mp_size_t n)
{
  mpz_t r;
  int res;

  mpz_init_set_str (r, ref, 16);
  while (n > 0 && xp[n-1] == 0)
    n--;
  
  res = (mpz_limbs_cmp (r, xp, n) == 0);
  mpz_clear (r);
  return res;
}

void
write_mpn (FILE *f, int base, const mp_limb_t *xp, mp_size_t n)
{
  mpz_t t;
  mpz_out_str (f, base, mpz_roinit_n (t,xp, n));
}

void
test_ecc_point (const struct ecc_curve *ecc,
		const struct ecc_ref_point *ref,
		const mp_limb_t *p)
{
  if (! (test_mpn (ref->x, p, ecc->p.size)
	 && test_mpn (ref->y, p + ecc->p.size, ecc->p.size) ))
    {
      fprintf (stderr, "Incorrect point!\n"
	       "got: x = ");
      write_mpn (stderr, 16, p, ecc->p.size);
      fprintf (stderr, "\n"
	       "     y = ");
      write_mpn (stderr, 16, p + ecc->p.size, ecc->p.size);
      fprintf (stderr, "\n"
	       "ref: x = %s\n"
	       "     y = %s\n",
	       ref->x, ref->y);
      abort();
    }
}

void
test_ecc_mul_a (unsigned curve, unsigned n, const mp_limb_t *p)
{
  /* For each curve, the points 2 g, 3 g and 4 g */
  static const struct ecc_ref_point ref[6][3] = {
    { { "dafebf5828783f2ad35534631588a3f629a70fb16982a888",
	"dd6bda0d993da0fa46b27bbc141b868f59331afa5c7e93ab" },
      { "76e32a2557599e6edcd283201fb2b9aadfd0d359cbb263da",
	"782c37e372ba4520aa62e0fed121d49ef3b543660cfd05fd" },
      { "35433907297cc378b0015703374729d7a4fe46647084e4ba",
	"a2649984f2135c301ea3acb0776cd4f125389b311db3be32" }
    },
    { { "706a46dc76dcb76798e60e6d89474788d16dc18032d268fd1a704fa6",
	"1c2b76a7bc25e7702a704fa986892849fca629487acf3709d2e4e8bb" },
      { "df1b1d66a551d0d31eff822558b9d2cc75c2180279fe0d08fd896d04",
	"a3f7f03cadd0be444c0aa56830130ddf77d317344e1af3591981a925" },
      { "ae99feebb5d26945b54892092a8aee02912930fa41cd114e40447301",
	"482580a0ec5bc47e88bc8c378632cd196cb3fa058a7114eb03054c9" },
    },
    { { "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978",
	"7775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1" },
      { "5ecbe4d1a6330a44c8f7ef951d4bf165e6c6b721efada985fb41661bc6e7fd6c",
	"8734640c4998ff7e374b06ce1a64a2ecd82ab036384fb83d9a79b127a27d5032" },
      { "e2534a3532d08fbba02dde659ee62bd0031fe2db785596ef509302446b030852",
	"e0f1575a4c633cc719dfee5fda862d764efc96c3f30ee0055c42c23f184ed8c6" },
    },
    { { "8d999057ba3d2d969260045c55b97f089025959a6f434d651d207d19fb96e9e"
	"4fe0e86ebe0e64f85b96a9c75295df61",
	"8e80f1fa5b1b3cedb7bfe8dffd6dba74b275d875bc6cc43e904e505f256ab425"
	"5ffd43e94d39e22d61501e700a940e80" },
      { "77a41d4606ffa1464793c7e5fdc7d98cb9d3910202dcd06bea4f240d3566da6"
	"b408bbae5026580d02d7e5c70500c831",
	"c995f7ca0b0c42837d0bbe9602a9fc998520b41c85115aa5f7684c0edc111eac"
	"c24abd6be4b5d298b65f28600a2f1df1" },
      { "138251cd52ac9298c1c8aad977321deb97e709bd0b4ca0aca55dc8ad51dcfc9d"
	"1589a1597e3a5120e1efd631c63e1835",
	"cacae29869a62e1631e8a28181ab56616dc45d918abc09f3ab0e63cf792aa4dc"
	"ed7387be37bba569549f1c02b270ed67" },
    },
    { { "43"
	"3c219024277e7e682fcb288148c282747403279b1ccc06352c6e5505d769be97"
	"b3b204da6ef55507aa104a3a35c5af41cf2fa364d60fd967f43e3933ba6d783d",
	"f4"
	"bb8cc7f86db26700a7f3eceeeed3f0b5c6b5107c4da97740ab21a29906c42dbb"
	"b3e377de9f251f6b93937fa99a3248f4eafcbe95edc0f4f71be356d661f41b02"
      },
      { "1a7"
	"3d352443de29195dd91d6a64b5959479b52a6e5b123d9ab9e5ad7a112d7a8dd1"
	"ad3f164a3a4832051da6bd16b59fe21baeb490862c32ea05a5919d2ede37ad7d",
	"13e"
	"9b03b97dfa62ddd9979f86c6cab814f2f1557fa82a9d0317d2f8ab1fa355ceec"
	"2e2dd4cf8dc575b02d5aced1dec3c70cf105c9bc93a590425f588ca1ee86c0e5" },
      { "35"
	"b5df64ae2ac204c354b483487c9070cdc61c891c5ff39afc06c5d55541d3ceac"
	"8659e24afe3d0750e8b88e9f078af066a1d5025b08e5a5e2fbc87412871902f3",
	"82"
	"096f84261279d2b673e0178eb0b4abb65521aef6e6e32e1b5ae63fe2f19907f2"
	"79f283e54ba385405224f750a95b85eebb7faef04699d1d9e21f47fc346e4d0d" },
    },
    { { "36ab384c9f5a046c3d043b7d1833e7ac080d8e4515d7a45f83c5a14e2843ce0e",
	"2260cdf3092329c21da25ee8c9a21f5697390f51643851560e5f46ae6af8a3c9" },
      { "67ae9c4a22928f491ff4ae743edac83a6343981981624886ac62485fd3f8e25c",
	"1267b1d177ee69aba126a18e60269ef79f16ec176724030402c3684878f5b4d4" },
      { "203da8db56cff1468325d4b87a3520f91a739ec193ce1547493aa657c4c9f870",
	"47d0e827cb1595e1470eb88580d5716c4cf22832ea2f0ff0df38ab61ca32112f" },
    }
  };
  assert (curve < 6);
  assert (n <= 4);
  if (n == 0)
    {
      /* Makes sense for curve25519 only */
      const struct ecc_curve *ecc = ecc_curves[curve];
      assert (ecc->p.bit_size == 255);
      if (!mpn_zero_p (p, ecc->p.size)
	  || mpn_cmp (p + ecc->p.size, ecc->unit, ecc->p.size) != 0)
	{
	  fprintf (stderr, "Incorrect point (expected (0, 1))!\n"
		   "got: x = ");
	  write_mpn (stderr, 16, p, ecc->p.size);
	  fprintf (stderr, "\n"
		   "     y = ");
	  write_mpn (stderr, 16, p + ecc->p.size, ecc->p.size);
	  fprintf (stderr, "\n");
	  abort();
	}
    }
  else if (n == 1)
    {
      const struct ecc_curve *ecc = ecc_curves[curve];
      if (mpn_cmp (p, ecc->g, 2*ecc->p.size) != 0)
	{
	  fprintf (stderr, "Incorrect point (expected g)!\n"
		   "got: x = ");
	  write_mpn (stderr, 16, p, ecc->p.size);
	  fprintf (stderr, "\n"
		   "     y = ");
	  write_mpn (stderr, 16, p + ecc->p.size, ecc->p.size);
	  fprintf (stderr, "\n"
		   "ref: x = ");
	  write_mpn (stderr, 16, ecc->g, ecc->p.size);
	  fprintf (stderr, "\n"
		   "     y = ");
	  write_mpn (stderr, 16, ecc->g + ecc->p.size, ecc->p.size);
	  fprintf (stderr, "\n");
	  abort();
	}
    }
  else
    test_ecc_point (ecc_curves[curve], &ref[curve][n-2], p);
}

void
test_ecc_mul_h (unsigned curve, unsigned n, const mp_limb_t *p)
{
  const struct ecc_curve *ecc = ecc_curves[curve];
  mp_limb_t *np = xalloc_limbs (ecc_size_a (ecc));
  mp_limb_t *scratch = xalloc_limbs (ecc->h_to_a_itch);
  ecc->h_to_a (ecc, 0, np, p, scratch);

  test_ecc_mul_a (curve, n, np);

  free (np);
  free (scratch);
}

#endif /* WITH_HOGWEED */

