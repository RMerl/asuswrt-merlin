#include <stdio.h>

#ifdef PROTOTYPES
int main (void)
#else
main()
#endif
{
  printf("Hello from vforked-prog...\n");
  return 0;
}
