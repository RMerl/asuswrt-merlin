/* Test handle_group_exit () handling of a thread leader still alive with its
 * thread child calling exit_group () and proper passing of the process exit
 * code to the process parent of this whole thread group.
 *
 * gcc -o test/leaderkill test/leaderkill.c -Wall -ggdb2 -pthread;./test/leaderkill & pid=$!;sleep 1;strace -o x -q ./strace -f -p $pid
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
	sleep(1);
	exit(42);
}

static void *start1(void *arg)
{
	pause();
	/* NOTREACHED */
	assert(0);
}

int main(int argc, char *argv[])
{
	pthread_t thread0;
	pthread_t thread1;
	pid_t child, got_pid;
	int status;
	int i;

	sleep(2);

	child = fork();

	switch(child) {
	case -1:
		abort();
	case 0:
		i = pthread_create(&thread0, NULL, start0, NULL);
		assert(i == 0);
		i = pthread_create(&thread1, NULL, start1, NULL);
		assert(i == 0);
		pause();
		/* NOTREACHED */
		assert(0);
		break;
	default:
		got_pid = waitpid(child, &status, 0);
		assert(got_pid == child);
		assert(WIFEXITED(status));
		assert(WEXITSTATUS(status) == 42);
		puts("OK");
		exit(0);
	}

	/* NOTREACHED */
	abort();
}
