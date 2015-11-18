#include "testutils.h"

void
test_main(void)
{
  test_hash(&nettle_sha1, SDATA(""),
	    SHEX("DA39A3EE5E6B4B0D 3255BFEF95601890 AFD80709")); 

  test_hash(&nettle_sha1, SDATA("a"),
	    SHEX("86F7E437FAA5A7FC E15D1DDCB9EAEAEA 377667B8")); 

  test_hash(&nettle_sha1, SDATA("abc"),
	    SHEX("A9993E364706816A BA3E25717850C26C 9CD0D89D"));
  
  test_hash(&nettle_sha1, SDATA("abcdefghijklmnopqrstuvwxyz"),
	    SHEX("32D10C7B8CF96570 CA04CE37F2A19D84 240D3A89"));
  
  test_hash(&nettle_sha1, SDATA("message digest"),
	    SHEX("C12252CEDA8BE899 4D5FA0290A47231C 1D16AAE3")); 

  test_hash(&nettle_sha1,
	    SDATA("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		  "abcdefghijklmnopqrstuvwxyz0123456789"),
	    SHEX("761C457BF73B14D2 7E9E9265C46F4B4D DA11F940"));
  
  test_hash(&nettle_sha1,
	    SDATA("1234567890123456789012345678901234567890"
		  "1234567890123456789012345678901234567890"),
	    SHEX("50ABF5706A150990 A08B2C5EA40FA0E5 85554732"));

  /* Additional test vector, from Daniel Kahn Gillmor */
  test_hash(&nettle_sha1, SDATA("38"),
	    SHEX("5b384ce32d8cdef02bc3a139d4cac0a22bb029e8"));
}

/* These are intermediate values for the single sha1_compress call
   that results from the first testcase, SHA1(""). Each row is the
   values for A, B, C, D, E after the i:th round. The row i = -1 gives
   the initial values, and i = 99 gives the output values.

      i         A        B        C        D        E
     -1: 67452301 efcdab89 98badcfe 10325476 c3d2e1f0
      0: 67452301 7bf36ae2 98badcfe 10325476 1fb498b3
      1: 59d148c0 7bf36ae2 98badcfe 5d43e370 1fb498b3
     15: 40182905 4544b22e a13017ac ab703832 d8fd6547
     16: 50060a41 4544b22e a13017ac  6bf9173 d8fd6547
     17: 50060a41 4544b22e 28a9520e  6bf9173 f63f5951
     18: 50060a41  b3088dd 28a9520e c1afe45c f63f5951
     19: e758e8da  b3088dd 8a2a5483 c1afe45c f63f5951
     20: e758e8da 42cc2237 8a2a5483 c1afe45c 90eb9850
     21: b9d63a36 42cc2237 8a2a5483 7dbb787d 90eb9850
     38:  e47bc31 62273351 b201788b 413c1d9a 2aeeae62
     39: 9bdbdd71 62273351 ec805e22 413c1d9a 2aeeae62
     40: 9bdbdd71 5889ccd4 ec805e22 413c1d9a 95aa398b
     41: 66f6f75c 5889ccd4 ec805e22 5e28e858 95aa398b
     58: 2164303a 982bcbca e1afab22 c5a3382e af9292fa
     59: 9b9d2913 982bcbca b86beac8 c5a3382e af9292fa
     60: 9b9d2913 a60af2f2 b86beac8 c5a3382e d37db937
     61: e6e74a44 a60af2f2 b86beac8 85b9d227 d37db937
     78: c57a6345 6e9d9f84 666b8bc6 852dc41a ec052519
     79: 72f480ed 6e9d9f84 999ae2f1 852dc41a ec052519
     99: da39a3ee 5e6b4b0d 3255bfef 95601890 afd80709
     
*/
