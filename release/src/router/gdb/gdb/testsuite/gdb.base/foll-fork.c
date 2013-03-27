#include <stdio.h>
#include <unistd.h>

#ifdef PROTOTYPES
void callee (int i)
#else
void callee (i)
  int  i;
#endif
{
  printf("callee: %d\n", i);
}

#ifdef PROTOTYPES
int main (void)
#else
main ()
#endif
{
  int  pid;
  int  v = 5;

  pid = fork ();
  if (pid == 0)
    {
      v++;
      /* printf ("I'm the child!\n"); */
    }
  else
    {
      v--;
      /* printf ("I'm the proud parent of child #%d!\n", pid); */
    }
}
