#include <stdio.h>

int callee( x )
int x;
{
    int y = x * x;
    return (y - 2);
}

main()
{
    int i;
    for (i = 1; i < 10; i++)
        {
            printf( "%d ", callee( i ));
            
        }
    printf( " Goodbye!\n" );
    
}
