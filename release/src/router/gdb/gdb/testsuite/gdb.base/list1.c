#include <stdio.h>

#ifdef PROTOTYPES
void long_line (); int oof (int);
void bar (int x)
#else
void bar (x) int x;
#endif
{
    printf ("%d\n", x);

    long_line ();
}

static void
unused ()
{
    /* Not used for anything */
}
/* This routine has a very long line that will break searching in older versions of GDB.  */
#ifdef PROTOTYPES
void
#endif
long_line ()
{
  oof (67);

  oof (6789);

  oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*  5 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 10 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 15 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 20 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 25 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 30 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 35 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 40 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 45 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 50 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 55 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 60 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /* 65 */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (12); /*    */      oof (12);  oof (12);  oof (12);  oof (12);  oof (12);  oof (1234); /* 70 */
}
#ifdef PROTOTYPES
int oof (int n)
#else
oof (n) int n;
#endif
{
  return n + 1;
}
