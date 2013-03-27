#include "list0.h"

int main ()
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
    return 0;
}

static void
unused ()
{
    /* Not used for anything */
}
