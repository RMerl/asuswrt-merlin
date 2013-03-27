/* Basic time functionality test.  */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
int
main (void)
{
  struct timeval t_m = {0, 0};
  time_t t;

  if ((t = time (NULL)) == (time_t) -1
      || gettimeofday (&t_m, NULL) != 0
      || t_m.tv_sec == 0

      /* We assume there will be no delay between the time and
	 gettimeofday calls above, but allow a timer-tick to make the
	 seconds increase by one.  */
      || (t != t_m.tv_sec && t+1 != t_m.tv_sec))
    {
      printf ("fail\n");
      exit (1);
    }

  printf ("pass\n");
  exit (0);
}
