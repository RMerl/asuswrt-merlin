/*
#notarget: cris*-*-elf
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

/* Check that socketcall is suitably stubbed.  */

int main (void)
{
  int ret = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (ret != -1)
    {
      fprintf (stderr, "sock: %d\n", ret);
      abort ();
    }

  if (errno != ENOSYS)
    {
      perror ("unexpected");
      abort ();
    }

  printf ("pass\n");
  return 0;
}
