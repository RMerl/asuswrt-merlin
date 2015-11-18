#include "testutils.h"

static void
test_ecdsa (const struct ecc_curve *ecc,
	    /* Private key */
	    const char *sz,
	    /* Random nonce */
	    const char *sk,
	    /* Hash */
	    const struct tstring *h,
	    /* Expected signature */
	    const char *r, const char *s)
{
  struct dsa_signature ref;
  mpz_t z;
  mpz_t k;
  mp_limb_t *rp = xalloc_limbs (ecc->p.size);
  mp_limb_t *sp = xalloc_limbs (ecc->p.size);
  mp_limb_t *scratch = xalloc_limbs (ecc_ecdsa_sign_itch (ecc));

  dsa_signature_init (&ref);

  mpz_init_set_str (z, sz, 16);
  mpz_init_set_str (k, sk, 16);

  ecc_ecdsa_sign (ecc, mpz_limbs_read_n (z, ecc->p.size),
		  mpz_limbs_read_n (k, ecc->p.size),
		  h->length, h->data, rp, sp, scratch);

  mpz_set_str (ref.r, r, 16);
  mpz_set_str (ref.s, s, 16);

  if (mpz_limbs_cmp (ref.r, rp, ecc->p.size) != 0
      || mpz_limbs_cmp (ref.s, sp, ecc->p.size) != 0)
    {
      fprintf (stderr, "_ecdsa_sign failed, bit_size = %u\n", ecc->p.bit_size);
      fprintf (stderr, "r     = ");
      write_mpn (stderr, 16, rp, ecc->p.size);
      fprintf (stderr, "\ns     = ");
      write_mpn (stderr, 16, sp, ecc->p.size);
      fprintf (stderr, "\nref.r = ");
      mpz_out_str (stderr, 16, ref.r);
      fprintf (stderr, "\nref.s = ");
      mpz_out_str (stderr, 16, ref.s);
      fprintf (stderr, "\n");
      abort();
    }

  free (rp);
  free (sp);
  free (scratch);

  dsa_signature_clear (&ref);
  mpz_clear (k);
  mpz_clear (z);
}

void
test_main (void)
{
  /* Test cases for the smaller groups, verified with a
     proof-of-concept implementation done for Yubico AB. */
  test_ecdsa (&nettle_secp_192r1,
	      "DC51D3866A15BACDE33D96F992FCA99D"
	      "A7E6EF0934E70975", /* z */

	      "9E56F509196784D963D1C0A401510EE7"
	      "ADA3DCC5DEE04B15", /* k */

	      SHEX("BA7816BF8F01CFEA414140DE5DAE2223"
		   "B00361A396177A9C"), /* h */

	      "8c478db6a5c131540cebc739f9c0a9a8"
	      "c720c2abdd14a891", /* r */

	      "a91fb738f9f175d72f9c98527e881c36"
	      "8de68cb55ffe589"); /* s */

  test_ecdsa (&nettle_secp_224r1,
	      "446df0a771ed58403ca9cb316e617f6b"
	      "158420465d00a69601e22858",  /* z */

	      "4c13f1905ad7eb201178bc08e0c9267b"
	      "4751c15d5e1831ca214c33f4",  /* z */

	      SHEX("1b28a611fe62ab3649350525d06703ba"
		   "4b979a1e543566fd5caa85c6"),  /* h */

	      "2cc280778f3d067df6d3adbe3a6aad63"
	      "bc75f08f5c5f915411902a99",  /* r */ 

	      "d0f069fd0f108eb07b7bbc54c8d6c88d"
	      "f2715c38a95c31a2b486995f"); /* s */

  /* From RFC 4754 */
  test_ecdsa (&nettle_secp_256r1,
	      "DC51D386 6A15BACD E33D96F9 92FCA99D"
	      "A7E6EF09 34E70975 59C27F16 14C88A7F",  /* z */

	      "9E56F509 196784D9 63D1C0A4 01510EE7"
	      "ADA3DCC5 DEE04B15 4BF61AF1 D5A6DECE",  /* k */

	      SHEX("BA7816BF 8F01CFEA 414140DE 5DAE2223"
		   "B00361A3 96177A9C B410FF61 F20015AD"),  /* h */
	      
	      "CB28E099 9B9C7715 FD0A80D8 E47A7707"
	      "9716CBBF 917DD72E 97566EA1 C066957C",  /* r */
	      "86FA3BB4 E26CAD5B F90B7F81 899256CE"
	      "7594BB1E A0C89212 748BFF3B 3D5B0315"); /* s */

  test_ecdsa (&nettle_secp_384r1,
	      "0BEB6466 34BA8773 5D77AE48 09A0EBEA"
	      "865535DE 4C1E1DCB 692E8470 8E81A5AF"
	      "62E528C3 8B2A81B3 5309668D 73524D9F",  /* z */

	      "B4B74E44 D71A13D5 68003D74 89908D56"
	      "4C7761E2 29C58CBF A1895009 6EB7463B"
	      "854D7FA9 92F934D9 27376285 E63414FA",  /* k */

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
	      "0065FDA3 409451DC AB0A0EAD 45495112"
	      "A3D813C1 7BFD34BD F8C1209D 7DF58491"
	      "20597779 060A7FF9 D704ADF7 8B570FFA"
	      "D6F062E9 5C7E0C5D 5481C5B1 53B48B37"
	      "5FA1", /* z */
	      
	      "00C1C2B3 05419F5A 41344D7E 4359933D"
	      "734096F5 56197A9B 244342B8 B62F46F9"
	      "373778F9 DE6B6497 B1EF825F F24F42F9"
	      "B4A4BD73 82CFC337 8A540B1B 7F0C1B95"
	      "6C2F", /* k */

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

  /* Non-standard ecdsa using curve25519. Not interop-tested with
     anything else. */
  test_ecdsa (&_nettle_curve25519,
	      "1db511101b8fd16f e0212c5679ef53f3"
	      "323bde77f9efa442 617314d576d1dbcb", /* z */
	      "aa2fa8facfdc3a99 ec466d41a2c9211c"
	      "e62e1706f54037ff 8486e26153b0fa79", /* k */
	      SHEX("e99df2a098c3c590 ea1e1db6d9547339"
		   "ae760d5331496119 5d967fd881e3b0f5"), /* h */
	      " 515c3a485f57432 0daf3353a0d08110"
	      "64157c556296de09 4132f74865961b37", /* r */
	      "  78f23367291b01 3fc430fb09322d95"
	      "4384723649868d8e 88effc7ac8b141d7"); /* s */
}
