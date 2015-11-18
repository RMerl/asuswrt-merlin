#include "testutils.h"
#include "poly1305.h"

static void
update (void *ctx, nettle_hash_update_func *f,
	const struct tstring *msg,
	unsigned length)
{
  for (; length > msg->length; length -= msg->length)
    f(ctx, msg->length, msg->data);
  f(ctx, length, msg->data);
}

static void
check_digest (const char *name, void *ctx, nettle_hash_digest_func *f,
	      const struct tstring *msg, unsigned length,
	      unsigned tag_length, const uint8_t *ref)
{
  uint8_t tag[16];
  f(ctx, tag_length, tag);
  if (memcmp (tag, ref, tag_length) != 0)
    {
      printf ("%s failed\n", name);
      printf ("msg: "); print_hex (msg->length, msg->data);
      printf ("length: %u\n", length);
      printf ("tag: "); print_hex (tag_length, tag);
      printf ("ref: "); print_hex (tag_length, ref);
      abort ();
    }

}

static void
test_poly1305 (const struct tstring *key,
	   const struct tstring *nonce,
	   const struct tstring *msg,
	   unsigned length,
	   const struct tstring *ref)
{
  struct poly1305_aes_ctx ctx;

  ASSERT (key->length == POLY1305_AES_KEY_SIZE);
  ASSERT (ref->length == POLY1305_AES_DIGEST_SIZE);

  poly1305_aes_set_key (&ctx, key->data);
  poly1305_aes_set_nonce (&ctx, nonce->data);

  update(&ctx, (nettle_hash_update_func *) poly1305_aes_update, msg, length);

  check_digest ("poly1305-aes", &ctx, (nettle_hash_digest_func *) poly1305_aes_digest,
		msg, length, 16, ref->data);
}

void
test_main(void)
{
  /* From Bernstein's paper. */
  test_poly1305
   (SHEX("75deaa25c09f208e1dc4ce6b5cad3fbfa0f3080000f46400d0c7e9076c834403"),
    SHEX("61ee09218d29b0aaed7e154a2c5509cc"),
    SHEX(""), 0,
    SHEX("dd3fab2251f11ac759f0887129cc2ee7"));

  test_poly1305
   (SHEX("ec074c835580741701425b623235add6851fc40c3467ac0be05cc20404f3f700"),
    SHEX("fb447350c4e868c52ac3275cf9d4327e"),
    SHEX("f3f6"), 2,
    SHEX("f4c633c3044fc145f84f335cb81953de"));

  test_poly1305
   (SHEX("6acb5f61a7176dd320c5c1eb2edcdc74"
         "48443d0bb0d21109c89a100b5ce2c208"),
    SHEX("ae212a55399729595dea458bc621ff0e"),
    SHEX("663cea190ffb83d89593f3f476b6bc24"
         "d7e679107ea26adb8caf6652d0656136"), 32,
    SHEX("0ee1c16bb73f0f4fd19881753c01cdbe"));

  test_poly1305
   (SHEX("e1a5668a4d5b66a5f68cc5424ed5982d12976a08c4426d0ce8a82407c4f48207"),
    SHEX("9ae831e743978d3a23527c7128149e3a"),
    SHEX("ab0812724a7f1e342742cbed374d94d136c6b8795d45b3819830f2c04491"
         "faf0990c62e48b8018b2c3e4a0fa3134cb67fa83e158c994d961c4cb2109"
         "5c1bf9"), 63,
    SHEX("5154ad0d2cb26e01274fc51148491f1b"));

}
