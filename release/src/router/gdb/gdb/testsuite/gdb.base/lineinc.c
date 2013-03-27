/* The following is written to tickle a specific bug in the macro
   table code (now hopefully fixed), which doesn't insert new included
   files in the #including file's list in the proper place.  They
   should be sorted by the number of the line which #included them, in
   increasing order, but the sense of the comparison was reversed, so
   the list ends up being built backwards.  This isn't a problem by
   itself, but the code to pick new, non-conflicting line numbers for
   headers alleged to be #included at the same line as some other
   header assumes that the list's line numbers are in ascending order.

   So, given the following input, lineinc1.h gets added to lineinc.c's
   #inclusion list first, at line 10.  When the debug info reader
   tries to add lineinc2.h at line 10 as well, the code will notice the
   duplication --- since there's only one extant element in the list,
   it'll find it --- and insert it after lineinc1.h, with line 11.
   Since the code is putting the list in order of descending
   #inclusion line number, the list is now out of order.  When we try
   to #include lineinc3.h at line 11, we won't notice the duplication.  */

#line 10
#include "lineinc1.h"
#line 10
#include "lineinc2.h"
#line 11
#include "lineinc3.h"

int
main (int argc, char **argv)
{
}
