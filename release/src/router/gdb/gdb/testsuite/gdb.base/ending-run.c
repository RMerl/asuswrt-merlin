/* Test program for <next-at-end> and
 * <leaves-core-file-on-quit> bugs.
 */
#include <stdio.h>
#include <stdlib.h>

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

    int *p;
    int i;

    p = (int *) malloc( 4 );

    for (i = 1; i < 10; i++)
        {
            printf( "%d ", callee( i ));
            fflush (stdout);
        }
    printf( " Goodbye!\n" ); fflush (stdout);
    return 0;
}
