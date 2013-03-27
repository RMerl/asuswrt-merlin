#include <stdio.h>

#ifdef PROTOTYPES
int callee (int x)
#else
int callee( x )
int x;
#endif
{
    int y = x * x;
    return (y - 2);
}

int main()
{
    int i;
    for (i = 1; i < 10; i++)
        {
            printf( "%d ", callee( i ));
            
        }
    printf( " Goodbye!\n" );
    return 0;
}
/* This routine exists only for aCC.  The way we compile this test is
   that we use aCC for the actual compile into the object file but then
   use ld directly for the link.  When we do this, we get an undefined
   symbol _main().  Therefore, for aCC, we have this routine in here and
   ld is happy.  */

#ifdef __cplusplus
extern "C" {
void _main()
{
}
}
#endif
