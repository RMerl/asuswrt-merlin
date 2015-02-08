/*
 * Simple user-land program instead of real "init"
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <asm/ioctls.h>
#include <sys/wait.h>

#include <linux/reboot.h>


static volatile unsigned long long counter ;

void
main_test_speed(unsigned sec, unsigned n)
{
	int pfd[2];
	unsigned long start ;
	unsigned i;

	printf("Testing speed with %d threads\n", n );
	start = time(NULL);
	counter = 0;
	pipe( pfd );

	for(i = 0; i < n; i ++ )
		{
		int pid ;
		if( (pid = fork()) != 0 )
			continue;
		/* Child process */
		close(pfd[0]);
		while( time(NULL) < (start + sec ) )
			counter ++ ;
		printf("Counter %llu\n", counter );
		write( pfd[1], (void *) &counter, sizeof(counter) );
		exit(0);
		}
	/* Children running, parent waits */
	close(pfd[1]);
	while( time(NULL) < (start + sec ) )
		sleep(1);

	/* Retreive child exist status */
	for(counter = i = 0; i < n; i ++ )
		{
		unsigned long long cnt ;
		int res;
		if( read(pfd[0], &cnt, sizeof(cnt)) == sizeof(cnt))
			counter += cnt ;
		wait( &res );
		printf("child %d status %d\n", i, WEXITSTATUS(res));
		}
	printf("Average count %llu, or %d per sec \n",
		counter / n, (int) (counter / n / sec) );
}


int
main(int argc, char ** argv )
{
	unsigned i, j;
	const unsigned seconds = 60;
	const unsigned dt = 5;
	char buf[128];

	printf("Howdy from user-land! ! ! !\n");
	
	for(j = i = 0; i < seconds; i += dt )
		{
		sleep(dt);
		printf("Time is %d sec\n", time(NULL) );
		ioctl(0, FIONREAD, &j);
		if(j > 0 )
			{
			memset(buf, 0, sizeof(buf));
			j = read(0, buf, sizeof(buf));
			printf("Received %d characters from input:\n", j);
			printf("'%s'\n", buf );
			}
		}

sleep(2);
main_test_speed( 30, 1 );
sleep(2);
main_test_speed( 30, 2 );
sleep(2);
main_test_speed( 30, 3 );
sleep(2);
main_test_speed( 30, 4 );

		ioctl(0, FIONREAD, &j);
		if(j > 0 )
			{
			memset(buf, 0, sizeof(buf));
			j = read(0, buf, sizeof(buf));
			printf("Received %d characters from input:\n", j);
			printf("'%s'\n", buf );
			}
sleep(1);
printf("Done couning, commiting suicide...\n");
sleep(3);
reboot( LINUX_REBOOT_CMD_RESTART );
sleep(5);
printf("reboot failed: %s\n", strerror(errno) );
}
