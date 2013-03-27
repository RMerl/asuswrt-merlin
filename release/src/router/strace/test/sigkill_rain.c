/* This test is not yet added to Makefile */

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

static const struct sockaddr sa;

int main(int argc, char *argv[])
{
	int loops;
	int pid;
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, NULL);

	loops = 999;
	if (argv[1])
		loops = atoi(argv[1]);

	while (--loops >= 0) {
		pid = fork();

		if (pid < 0)
			exit(1);

		if (!pid) {
			/* child */
			int child = getpid();

			loops = 99;
			while (--loops) {
				pid = fork();

				if (pid < 0)
					exit(1);

				if (!pid) {
					/* grandchild: kill child */
					kill(child, SIGKILL);
					exit(0);
				}

				/* Add various syscalls you want to test here.
				 * strace will decode them and suddenly find
				 * process disappearing.
				 * But leave at least one case "empty", so that
				 * "kill grandchild" happens quicker.
				 * This produces cases when strace can't even
				 * decode syscall number before process dies.
				 */
				switch (loops & 1) {
				case 0:
					break; /* intentional empty */
				case 1:
					sendto(-1, "Hello cruel world", 17, 0, &sa, sizeof(sa));
					break;
				}

				/* kill grandchild */
				kill(pid, SIGKILL);
			}

			exit(0);
		}

		/* parent */
		wait(NULL);
	}

	return 0;
}
