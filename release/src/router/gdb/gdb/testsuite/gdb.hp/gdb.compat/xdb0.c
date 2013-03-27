#include "xdb0.h"

main ()
{
    int x;
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    x = 0;
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
    foo (x++);
}

static void
unused ()
{
    /* Not used for anything */
}
