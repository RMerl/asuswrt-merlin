#include "testutils.h"
#include "sexp.h"

#include "buffer.h"

#if WITH_HOGWEED
# include "bignum.h"
#endif

void
test_main(void)
{
  struct nettle_buffer buffer;

  {
    const uint8_t e[] = "(3:foo(3:bar17:xxxxxxxxxxxxxxxxx))";

    nettle_buffer_init(&buffer);
    ASSERT(sexp_format(&buffer, "(%0s(%0s%0s))",
		       "foo", "bar", "xxxxxxxxxxxxxxxxx")
	   == strlen(e));
    
    ASSERT(sexp_format(NULL, "(%0s(%0s%0s))",
		       "foo", "bar", "xxxxxxxxxxxxxxxxx")
	   == strlen(e));
    
    ASSERT(buffer.size == strlen(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }
  {
    const uint8_t e[] = "{KDM6Zm9vKDM6YmFyMTc6eHh4eHh4eHh4eHh4eHh4eHgpKQ==}";

    nettle_buffer_init(&buffer);
    ASSERT(sexp_transport_format(&buffer, "(%0s(%0s%0s))",
		       "foo", "bar", "xxxxxxxxxxxxxxxxx")
	   == strlen(e));
    
    ASSERT(sexp_transport_format(NULL, "(%0s(%0s%0s))",
				 "foo", "bar", "xxxxxxxxxxxxxxxxx")
	   == strlen(e));
    
    ASSERT(buffer.size == strlen(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }
  {
    const uint8_t e[] = "1:\0""1:a2:bc3:def4:ghij5:\x00\xDE\xAD\xBE\xEF";

    nettle_buffer_init(&buffer);  
    ASSERT(sexp_format(&buffer, "%i%i%i%i%i%i",
		       0, 0x61, 0x6263, 0x646566, 0x6768696a, 0xDEADBEEF)
	   == LLENGTH(e));
    
    ASSERT(buffer.size == LLENGTH(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }

  {
    const uint8_t e[] = "(3:foo(4:bar))";
    
    nettle_buffer_init(&buffer);  
    ASSERT(sexp_format(&buffer, "(%0s%l)",
		       "foo", (size_t) 7, "(4:bar)")
	   == strlen(e));
    
    ASSERT(buffer.size == strlen(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }

  {
    const uint8_t e[] = "([1:t]3:foo3:bar[6:gazonk]3:baz1:q)";

    nettle_buffer_init(&buffer);
    ASSERT(sexp_format(&buffer, "(%0t%0s%0s%0t%0s%0t%0s)",
		       "t", "foo", "bar", "gazonk", "baz", NULL, "q")
	   == strlen(e));

    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }

  /* Try literals */
  {
    const uint8_t e[] = "(3:foo(3:bar17:xxxxxxxxxxxxxxxxx))";

    nettle_buffer_init(&buffer);
    ASSERT(sexp_format(&buffer, "(%0s(bar%0s))",
		       "foo", "xxxxxxxxxxxxxxxxx")
	   == strlen(e));
    
    ASSERT(sexp_format(NULL, "(%0s(bar %0s))",
		       "foo", "xxxxxxxxxxxxxxxxx")
	   == strlen(e));
    
    ASSERT(buffer.size == strlen(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }
  {
    const uint8_t e[] = "(3:foo(3:bar17:xxxxxxxxxxxxxxxxx))";

    nettle_buffer_init(&buffer);
    ASSERT(sexp_format(&buffer, "(%0s(bar xxxxxxxxxxxxxxxxx))",
		       "foo")
	   == strlen(e));
    
    ASSERT(sexp_format(NULL, "(%0s(bar xxxxxxxxxxxxxxxxx))",
		       "foo")
	   == strlen(e));
    
    ASSERT(buffer.size == strlen(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }

  /* Literal parenthesis */
  {
    const uint8_t e[] = ")3:foo(3:bar";

    nettle_buffer_init(&buffer);
    ASSERT(sexp_format(&buffer, "%)foo%(%s", (size_t) 3, "bar")
	   == strlen(e));
    
    ASSERT(sexp_format(NULL, "%)foo%(%s", (size_t) 3, "bar")
	   == strlen(e));
    
    ASSERT(buffer.size == strlen(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));
    nettle_buffer_clear(&buffer);
  }
  
#if WITH_HOGWEED
  {
    mpz_t x;
    mpz_t y;
    mpz_t z;
    
    const uint8_t e[] =
      "(3:foo(3:bar1:\xff""11:abcdefghijk13:\0\x81""abcdefghijk))";

    mpz_init_set_si(x, -1);
    nettle_mpz_init_set_str_256_u(y, 11, "abcdefghijk");
    nettle_mpz_init_set_str_256_u(z, 12, "\x81""abcdefghijk");
    nettle_buffer_init(&buffer);

    ASSERT(sexp_format(&buffer, "(%0s(%0s%b%b%b))",
		     "foo", "bar", x, y, z)
	   == LLENGTH(e));

    ASSERT(sexp_format(NULL, "(%0s(%0s%b%b%b))",
		     "foo", "bar", x, y, z)
	   == LLENGTH(e));
    
    ASSERT(buffer.size == LLENGTH(e));
    ASSERT(MEMEQ(buffer.size, buffer.contents, e));

    nettle_buffer_clear(&buffer);
    mpz_clear(x);
    mpz_clear(y);
    mpz_clear(z);
  }
#endif /* WITH_HOGWEED */
}
