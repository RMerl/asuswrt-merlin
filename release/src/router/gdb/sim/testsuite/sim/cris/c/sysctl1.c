/*
#notarget: cris*-*-elf
*/

#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* I can't seem to include the right things, so we do it brute force.  */
int main (void)
{
  static int sysctl_args[] = { 1, 4 };
  size_t x = 8;

  struct __sysctl_args {
    int *name;
    int nlen;
    void *oldval;
    size_t *oldlenp;
    void *newval;
    size_t newlen;
    unsigned long __unused[4];
  } scargs
      =
    {
     sysctl_args,
     sizeof (sysctl_args) / sizeof (sysctl_args[0]),
     (void *) -1, &x, NULL, 0
    };

  if (syscall (SYS__sysctl, &scargs) != -1
      || errno != EFAULT)
    abort ();
  printf ("pass\n");
  exit (0);
}
