/* Test exit of a child of a TCB_EXITING child where the toplevel process starts
 * waiting on it.  The middle one gets detached and strace must update the
 * toplevel process'es number of attached children to 0.
 *
 * gcc -o test/childthread test/childthread.c -Wall -ggdb2 -pthread;./strace -f ./test/childthread
 * It must print: write(1, "OK\n", ...
 */

#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

static void *start0(void *arg)
{
	pause();
	/* NOTREACHED */
	assert(0);
}

int main(int argc, char *argv[])
{
	pthread_t thread0;
	pid_t child, got_pid;
	int status;
	int i;

	child = fork();

	switch(child) {
	case -1:
		assert(0);
	case 0:
		i = pthread_create(&thread0, NULL, start0, NULL);
		assert(i == 0);
		/* The thread must be initialized, it becomes thread-child of this
		   process-child (child of a child of the toplevel process).  */
		sleep(1);
		/* Here the child TCB cannot be deallocated as there still exist
		 * children (the thread child in START0).  */
		exit(42);
		/* NOTREACHED */
		assert(0);
	default:
		/* We must not be waiting in WAITPID when the child double-exits.  */
		sleep(2);
		/* PID must be -1.  */
		got_pid = waitpid(-1, &status, 0);
		assert(got_pid == child);
		assert(WIFEXITED(status));
		assert(WEXITSTATUS(status) == 42);
		puts("OK");
		exit(0);
	}

	/* NOTREACHED */
	assert(0);
}
