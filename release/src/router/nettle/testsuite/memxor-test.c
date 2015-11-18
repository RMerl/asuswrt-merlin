#include "testutils.h"
#include "memxor.h"

#define MAX_SIZE 256
#define ALIGN_SIZE 16

#if HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
# define ROUND_DOWN(x) ((x) & (-ALIGN_SIZE))
# define ROUND_UP(x) ROUND_DOWN((x)+(ALIGN_SIZE-1))
enum mark_type { MARK_SRC, MARK_DST };

static void
test_mark (enum mark_type type,
	   const uint8_t *block, size_t block_size,
	   const uint8_t *p, size_t size)
{
  VALGRIND_MAKE_MEM_NOACCESS(block, p - block);
  if (type == MARK_DST)
    VALGRIND_MAKE_MEM_UNDEFINED(p, size);
  VALGRIND_MAKE_MEM_NOACCESS(p + size,
			     (block + block_size) - (p + size));
}

#define test_unmark(block, size) \
  VALGRIND_MAKE_MEM_DEFINED((block), (size))
#else
# define test_mark(type, block, block_size, start, size)
# define test_unmark(block, size)
#endif

static uint8_t *
set_align(uint8_t *buf, unsigned align)
{
  unsigned offset;
  /* An extra redzone char at the beginning */
  buf++;
  offset = (uintptr_t) (buf) % ALIGN_SIZE;

  if (offset < align)
    buf += (align - offset);
  else if (offset > align)
    buf += (align + ALIGN_SIZE - offset);

  return buf;  
}

static void
test_memxor (const uint8_t *a, const uint8_t *b, const uint8_t *c,
	     size_t size, unsigned align_dst, unsigned align_src)
{
  uint8_t dst_buf[MAX_SIZE + ALIGN_SIZE + 1];
  uint8_t src_buf[MAX_SIZE + ALIGN_SIZE + 1];

  uint8_t *dst = set_align (dst_buf, align_dst);
  uint8_t *src = set_align (src_buf, align_src);

  if (verbose)
    fprintf(stderr, "size = %d, align_dst = %d, align_src = %d\n",
	      (int) size, align_dst, align_src);

  memcpy (dst, a, size);
  dst[-1] = 17;
  dst[size] = 17;

  memcpy (src, b, size);
  test_mark (MARK_SRC, src_buf, sizeof (src_buf), src, size);
  test_mark (MARK_SRC, dst_buf, sizeof (dst_buf), dst, size);

  memxor (dst, src, size);
  ASSERT (MEMEQ (size, dst, c));
  
  test_unmark(src_buf, sizeof (src_buf));
  test_unmark(dst_buf, sizeof (src_buf));
  ASSERT (dst[-1] == 17);
  ASSERT (dst[size] == 17);
}

static void
test_memxor3 (const uint8_t *ain, const uint8_t *bin, const uint8_t *c,
	      size_t size, unsigned align_dst, unsigned align_a, unsigned align_b)
{
  uint8_t dst_buf[MAX_SIZE + ALIGN_SIZE + 1];
  uint8_t a_buf[MAX_SIZE + ALIGN_SIZE + 1];
  uint8_t b_buf[MAX_SIZE + ALIGN_SIZE + 1];

  uint8_t *dst = set_align (dst_buf, align_dst);
  uint8_t *a = set_align (a_buf, align_a);
  uint8_t *b = set_align (b_buf, align_b);

  if (verbose)
    fprintf(stderr, "size = %d, align_dst = %d, align_a = %d, align_b = %d\n",
	    (int) size, align_dst, align_a, align_b);

  memset (dst, 0, size);
  dst[-1] = 17;
  dst[size] = 17;

  memcpy (a, ain, size);
  memcpy (b, bin, size);
  test_mark (MARK_SRC, a_buf, sizeof(a_buf), a, size);
  test_mark (MARK_SRC, b_buf, sizeof(b_buf), b, size);
  test_mark (MARK_DST, dst_buf, sizeof(dst_buf), dst, size);

  memxor3 (dst, a, b, size);
  ASSERT (MEMEQ (size, dst, c));

  test_unmark (a_buf, sizeof(a_buf));
  test_unmark (b_buf, sizeof(b_buf));
  test_unmark (dst_buf, sizeof(dst_buf));

  ASSERT (dst[-1] == 17);
  ASSERT (dst[size] == 17);
}

void
test_main(void)
{
  const uint8_t *a = H("ecc8737f 38f2f9e8 86b9d84c 42a9c7ef"
		       "27a50860 49c6be97 c5cc6c35 3981b367"
		       "f8b4397b 951e3b2f 35749fe1 25884fa6"
		       "9361c97a ab1c6cce 494efb5a 1f108411"
		       "21dc6386 e81b2410 2f04c29d e0ca1135"
		       "c9f96f2e bb5b2e2d 8cb45df9 50c4755a"
		       "362b7ead 4b930010 cbc69834 66221ba8"
		       "c0b8d7ac 7ec3b700 6bdb1a3b 599f3e76"
		       "a7e66a29 ee1fb98c 60a66c9e 0a1d9c49"
		       "6367afc7 362d6ae1 f8799443 17e2b1a1"
		       "ff1cc03c 9e2728ca a1f6598f 5a61bd56"
		       "0826effc f3499da7 119249b6 fd643cd4"
		       "2e7c74b0 f775fda4 a5617138 1e8520bf"
		       "f17de57a decc36b6 9eceee6e d448f592"
		       "be77a67a 1b91a5b3 62fab868 dcb046f6"
		       "394b5335 b2eaa351 fc4456e4 35bb9c54");

  const uint8_t *b = H("cac458ad fe87e226 6cb0ce3d cfa5cb3b"
		       "963d0034 5811bb9e acf4675b 7464f800"
		       "4b1bcff2 b2fa5dd0 0576aea6 888b8150"
		       "bcba48f1 49bc33d2 e138b0d0 a29b486e"
		       "f7e143c6 f9959596 6aaa4493 b0bea6f8"
		       "1d778513 a3bfec7e 70cfe6a7 e31ad041"
		       "5fe3371b 63aba991 dab9a3db 66310ebc"
		       "24c2765d a722a131 2fc4d366 1f2e3388"
		       "7e5b26d5 7b34bf4c 655d19da d1335362"
		       "2fbc0d5d cc68c811 ef735c20 352986ef"
		       "f47ac5c9 afa77f5a 20da6dd3 eb9dfb34"
		       "0cdbf792 caf0d633 61d908da a4c0f2a9"
		       "be7a573e 3b8d161c 47fc19be e47d7edc"
		       "e5f00dae f64cbbb4 a081e1f0 381833d8"
		       "30d302ff eed61887 3390d6b2 0048ac32"
		       "9c6b2981 a224dcc1 6b1feebe 15834b1a");

  const uint8_t *c = H("260c2bd2 c6751bce ea091671 8d0c0cd4"
		       "b1980854 11d70509 69380b6e 4de54b67"
		       "b3aff689 27e466ff 30023147 ad03cef6"
		       "2fdb818b e2a05f1c a8764b8a bd8bcc7f"
		       "d63d2040 118eb186 45ae860e 5074b7cd"
		       "d48eea3d 18e4c253 fc7bbb5e b3dea51b"
		       "69c849b6 2838a981 117f3bef 00131514"
		       "e47aa1f1 d9e11631 441fc95d 46b10dfe"
		       "d9bd4cfc 952b06c0 05fb7544 db2ecf2b"
		       "4cdba29a fa45a2f0 170ac863 22cb374e"
		       "0b6605f5 31805790 812c345c b1fc4662"
		       "04fd186e 39b94b94 704b416c 59a4ce7d"
		       "9006238e ccf8ebb8 e29d6886 faf85e63"
		       "148de8d4 28808d02 3e4f0f9e ec50c64a"
		       "8ea4a485 f547bd34 516a6eda dcf8eac4"
		       "a5207ab4 10ce7f90 975bb85a 2038d74e");

  const int size[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 23, 24, 25, 30, 31, 32, 33, 34, 35, 36, 37,
    250, 251, 252, 253,254, 255, 256, -1
  };
  
  unsigned i, align_dst, align_a, align_b;
  for (i = 0; size[i] >= 0; i++)
    for (align_dst = 0; align_dst < ALIGN_SIZE; align_dst++)
      for (align_a = 0; align_a < ALIGN_SIZE; align_a++)
	{
	  test_memxor (a, b, c, size[i], align_dst, align_a);
	  for (align_b = 0; align_b < ALIGN_SIZE; align_b++)
	    test_memxor3 (a, b, c, size[i], align_dst, align_a, align_b);
	}
}
