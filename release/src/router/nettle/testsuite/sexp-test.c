#include "testutils.h"
#include "sexp.h"

void
test_main(void)
{
  struct sexp_iterator i;
  uint32_t x;
  
  ASSERT(sexp_iterator_first(&i, LDATA("")));
  ASSERT(i.type == SEXP_END);

  ASSERT(sexp_iterator_first(&i, LDATA("()")));
  ASSERT(i.type == SEXP_LIST
	 && sexp_iterator_enter_list(&i)
	 && i.type == SEXP_END
	 && sexp_iterator_exit_list(&i)
	 && i.type == SEXP_END);

  ASSERT(sexp_iterator_first(&i, LDATA("(")));
  ASSERT(i.type == SEXP_LIST
	 && !sexp_iterator_enter_list(&i));

  /* Check integers. */
  ASSERT(sexp_iterator_first(&i, LDATA("1:\0"
				       "1:\x11"
				       "2:\x00\x11"
				       "2:\x00\x80"
				       "5:\x00\xaa\xbb\xcc\xdd")));
  ASSERT(sexp_iterator_get_uint32(&i, &x) && x == 0);
  ASSERT(sexp_iterator_get_uint32(&i, &x) && x == 0x11);
  ASSERT(sexp_iterator_get_uint32(&i, &x) && x == 0x11);
  ASSERT(sexp_iterator_get_uint32(&i, &x) && x == 0x80);
  ASSERT(sexp_iterator_get_uint32(&i, &x) && x == 0xaabbccdd);

  ASSERT(sexp_iterator_first(&i, LDATA("3:foo0:[3:bar]12:xxxxxxxxxxxx")));
  ASSERT(i.type == SEXP_ATOM
	 && !i.display_length && !i.display
	 && i.atom_length == 3 && MEMEQ(3, "foo", i.atom)

	 && sexp_iterator_next(&i) && i.type == SEXP_ATOM
	 && !i.display_length && !i.display
	 && !i.atom_length && i.atom

	 && sexp_iterator_next(&i) && i.type == SEXP_ATOM
	 && i.display_length == 3 && MEMEQ(3, "bar", i.display)
	 && i.atom_length == 12 && MEMEQ(12, "xxxxxxxxxxxx", i.atom)

	 && sexp_iterator_next(&i) && i.type == SEXP_END);
  
  /* Same data, transport encoded. */

  {
    struct tstring *s
      = tstring_data(LDATA("{Mzpmb28=} {MDo=} {WzM6YmFyXTEyOnh4eHh4eHh4eHh4eA==}"));
  ASSERT(sexp_transport_iterator_first (&i, s->length, s->data));
  ASSERT(i.type == SEXP_ATOM
	 && !i.display_length && !i.display
	 && i.atom_length == 3 && MEMEQ(3, "foo", i.atom)

	 && sexp_iterator_next(&i) && i.type == SEXP_ATOM
	 && !i.display_length && !i.display
	 && !i.atom_length && i.atom

	 && sexp_iterator_next(&i) && i.type == SEXP_ATOM
	 && i.display_length == 3 && MEMEQ(3, "bar", i.display)
	 && i.atom_length == 12 && MEMEQ(12, "xxxxxxxxxxxx", i.atom)

	 && sexp_iterator_next(&i) && i.type == SEXP_END);

  }
  {
    static const uint8_t *keys[2] = { "n", "e" };
    struct sexp_iterator v[2];
    
    ASSERT(sexp_iterator_first(&i, LDATA("((1:n2:xx3:foo)0:(1:y)(1:e))")));
    ASSERT(sexp_iterator_enter_list(&i)
	   && sexp_iterator_assoc(&i, 2, keys, v));

    ASSERT(v[0].type == SEXP_ATOM
	   && !v[0].display_length && !v[0].display
	   && v[0].atom_length == 2 && MEMEQ(2, "xx", v[0].atom)

	   && sexp_iterator_next(&v[0]) && v[0].type == SEXP_ATOM
	   && !v[0].display_length && !v[0].display
	   && v[0].atom_length == 3 && MEMEQ(3, "foo", v[0].atom)

	   && sexp_iterator_next(&v[0]) && v[0].type == SEXP_END);

    ASSERT(v[1].type == SEXP_END);

    ASSERT(sexp_iterator_first(&i, LDATA("((1:n))")));
    ASSERT(sexp_iterator_enter_list(&i)
	   && !sexp_iterator_assoc(&i, 2, keys, v));

    ASSERT(sexp_iterator_first(&i, LDATA("((1:n)(1:n3:foo))")));
    ASSERT(sexp_iterator_enter_list(&i)
	   && !sexp_iterator_assoc(&i, 2, keys, v));    
  }
}
