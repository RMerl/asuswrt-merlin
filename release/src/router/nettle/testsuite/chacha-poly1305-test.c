#include "testutils.h"
#include "nettle-internal.h"

void
test_main(void)
{
  /* From draft-irtf-cfrg-chacha20-poly1305-08 */
  test_aead (&nettle_chacha_poly1305, NULL,
	     SHEX("8081828384858687 88898a8b8c8d8e8f"
		  "9091929394959697 98999a9b9c9d9e9f"),
	     SHEX("50515253c0c1c2c3 c4c5c6c7"),
	     SHEX("4c61646965732061 6e642047656e746c"
		  "656d656e206f6620 74686520636c6173"
		  "73206f6620273939 3a20496620492063"
		  "6f756c64206f6666 657220796f75206f"
		  "6e6c79206f6e6520 74697020666f7220"
		  "7468652066757475 72652c2073756e73"
		  "637265656e20776f 756c642062652069"
		  "742e"),
	     SHEX("d31a8d34648e60db7b86afbc53ef7ec2"
		  "a4aded51296e08fea9e2b5a736ee62d6"
		  "3dbea45e8ca9671282fafb69da92728b"
		  "1a71de0a9e060b2905d6a5b67ecd3b36"
		  "92ddbd7f2d778b8c9803aee328091b58"
		  "fab324e4fad675945585808b4831d7bc"
		  "3ff4def08e4b7a9de576d26586cec64b"
		  "6116"),
	     /* The draft splits the nonce into a "common part" and an
		iv, and it seams the "common part" is the first 4
		bytes. */
	     SHEX("0700000040414243 44454647"),
	     SHEX("1ae10b594f09e26a 7e902ecbd0600691"));
}
