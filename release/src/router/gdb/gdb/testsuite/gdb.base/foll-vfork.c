#include <stdio.h>
#include <unistd.h>

#ifdef PROTOTYPES
int main (void)
#else
main ()
#endif
{
  int  pid;

  pid = vfork ();
  if (pid == 0) {
    printf ("I'm the child!\n");
    execlp ("gdb.base/vforked-prog", "gdb.base/vforked-prog", (char *)0);
  }
  else {
    printf ("I'm the proud parent of child #%d!\n", pid);
  }
}
