#include "testutils.h"

static void
test_ecdsa (const struct ecc_curve *ecc,
	    /* Public key */
	    const char *xs, const char *ys,
	    /* Hash */
	    struct tstring *h,
	    /* Valid signature */
	    const char *r, const char *s)
{
  struct ecc_point pub;
  struct dsa_signature signature;
  mpz_t x, y;

  ecc_point_init (&pub, ecc);
  dsa_signature_init (&signature);

  mpz_init_set_str (x, xs, 16);
  mpz_init_set_str (y, ys, 16);

  if (!ecc_point_set (&pub, x, y))
    die ("ecc_point_set failed.\n");

  mpz_set_str (signature.r, r, 16);
  mpz_set_str (signature.s, s, 16);

  if (!ecdsa_verify (&pub, h->length, h->data, &signature))
    {
      fprintf (stderr, "ecdsa_verify failed with valid signature.\n");
    fail:
      fprintf (stderr, "bit_size = %u\nx = ", ecc->p.bit_size);
      mpz_out_str (stderr, 16, x);
      fprintf (stderr, "\ny = ");
      mpz_out_str (stderr, 16, y);
      fprintf (stderr, "\ndigest ");
      print_hex (h->length, h->data);
      fprintf (stderr, "r = ");
      mpz_out_str (stderr, 16, signature.r);
      fprintf (stderr, "\ns = ");
      mpz_out_str (stderr, 16, signature.s);
      fprintf (stderr, "\n");
      abort();
    }

  mpz_combit (signature.r, ecc->p.bit_size / 3);
  if (ecdsa_verify (&pub, h->length, h->data, &signature))
    {
      fprintf (stderr, "ecdsa_verify unexpectedly succeeded with invalid signature.\n");
      goto fail;
    }
  mpz_combit (signature.r, ecc->p.bit_size / 3);
  
  mpz_combit (signature.s, 4*ecc->p.bit_size / 5);
  if (ecdsa_verify (&pub, h->length, h->data, &signature))
    {
      fprintf (stderr, "ecdsa_verify unexpectedly succeeded with invalid signature.\n");
      goto fail;
    }
  mpz_combit (signature.s, 4*ecc->p.bit_size / 5);

  h->data[2*h->length / 3] ^= 0x40;
  if (ecdsa_verify (&pub, h->length, h->data, &signature))
    {
      fprintf (stderr, "ecdsa_verify unexpectedly succeeded with invalid signature.\n");
      goto fail;
    }
  h->data[2*h->length / 3] ^= 0x40;
  if (!ecdsa_verify (&pub, h->length, h->data, &signature))
    {
      fprintf (stderr, "ecdsa_verify failed, internal testsuite error.\n");
      goto fail;
    }

  ecc_point_clear (&pub);
  dsa_signature_clear (&signature);
  mpz_clear (x);
  mpz_clear (y);  
}

void
test_main (void)
{
  /* From RFC 4754 */
  test_ecdsa (&nettle_secp_256r1,
	      "2442A5CC 0ECD015F A3CA31DC 8E2BBC70"
	      "BF42D60C BCA20085 E0822CB0 4235E970",  /* x */

	      "6FC98BD7 E50211A4 A27102FA 3549DF79"
	      "EBCB4BF2 46B80945 CDDFE7D5 09BBFD7D",  /* y */

	      SHEX("BA7816BF 8F01CFEA 414140DE 5DAE2223"
		   "B00361A3 96177A9C B410FF61 F20015AD"),  /* h */
	      
	      "CB28E099 9B9C7715 FD0A80D8 E47A7707"
	      "9716CBBF 917DD72E 97566EA1 C066957C",  /* r */
	      "86FA3BB4 E26CAD5B F90B7F81 899256CE"
	      "7594BB1E A0C89212 748BFF3B 3D5B0315"); /* s */

  test_ecdsa (&nettle_secp_384r1,
	      "96281BF8 DD5E0525 CA049C04 8D345D30"
	      "82968D10 FEDF5C5A CA0C64E6 465A97EA"
	      "5CE10C9D FEC21797 41571072 1F437922",  /* x */

	      "447688BA 94708EB6 E2E4D59F 6AB6D7ED"
	      "FF9301D2 49FE49C3 3096655F 5D502FAD"
	      "3D383B91 C5E7EDAA 2B714CC9 9D5743CA",  /* y */

	      SHEX("CB00753F 45A35E8B B5A03D69 9AC65007"
		   "272C32AB 0EDED163 1A8B605A 43FF5BED"
		   "8086072B A1E7CC23 58BAECA1 34C825A7"),  /* h */

	      "FB017B91 4E291494 32D8BAC2 9A514640"
	      "B46F53DD AB2C6994 8084E293 0F1C8F7E"
	      "08E07C9C 63F2D21A 07DCB56A 6AF56EB3",  /* r */
	      "B263A130 5E057F98 4D38726A 1B468741"
	      "09F417BC A112674C 528262A4 0A629AF1"
	      "CBB9F516 CE0FA7D2 FF630863 A00E8B9F"); /* s*/

  test_ecdsa (&nettle_secp_521r1,
	      "0151518F 1AF0F563 517EDD54 85190DF9"
	      "5A4BF57B 5CBA4CF2 A9A3F647 4725A35F"
	      "7AFE0A6D DEB8BEDB CD6A197E 592D4018"
	      "8901CECD 650699C9 B5E456AE A5ADD190"
	      "52A8", /* x */

	      "006F3B14 2EA1BFFF 7E2837AD 44C9E4FF"
	      "6D2D34C7 3184BBAD 90026DD5 E6E85317"
	      "D9DF45CA D7803C6C 20035B2F 3FF63AFF"
	      "4E1BA64D 1C077577 DA3F4286 C58F0AEA"
	      "E643", /* y */

	      SHEX("DDAF35A1 93617ABA CC417349 AE204131" 
		   "12E6FA4E 89A97EA2 0A9EEEE6 4B55D39A"
		   "2192992A 274FC1A8 36BA3C23 A3FEEBBD" 
		   "454D4423 643CE80E 2A9AC94F A54CA49F"), /* h */

	      "0154FD38 36AF92D0 DCA57DD5 341D3053" 
	      "988534FD E8318FC6 AAAAB68E 2E6F4339"
	      "B19F2F28 1A7E0B22 C269D93C F8794A92" 
	      "78880ED7 DBB8D936 2CAEACEE 54432055"
	      "2251", /* r */
	      "017705A7 030290D1 CEB605A9 A1BB03FF"
	      "9CDD521E 87A696EC 926C8C10 C8362DF4"
	      "97536710 1F67D1CF 9BCCBF2F 3D239534" 
	      "FA509E70 AAC851AE 01AAC68D 62F86647"
	      "2660"); /* s */

  test_ecdsa (&_nettle_curve25519,
	      /* Public key corresponding to the key in ecdsa-sign-test */
	      "59f8f317fd5f4e82 c02f8d4dec665fe1"
	      "230f83b8572638e1 b2ac34a30028e24d", /* x */
	      "1902a72dc1a6525a 811b9c1845978d56"
	      "fd97dce5e278ebdd ec695349d7e41498", /* y */
	      SHEX("e99df2a098c3c590 ea1e1db6d9547339"
		   "ae760d5331496119 5d967fd881e3b0f5"), /* h */
	      " 515c3a485f57432 0daf3353a0d08110"
	      "64157c556296de09 4132f74865961b37", /* r */
	      "  78f23367291b01 3fc430fb09322d95"
	      "4384723649868d8e 88effc7ac8b141d7"); /* s */
}
