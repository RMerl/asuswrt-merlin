/**
 * angel process for lighttpd 
 *
 * the purpose is the run as root all the time and handle:
 * - restart on crash
 * - spawn on HUP to allow graceful restart
 * - ...
 *
 * it has to stay safe and small to be trustable
 */

#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define BINPATH SBIN_DIR"/lighttpd"

static siginfo_t last_sigterm_info;
static siginfo_t last_sighup_info;

static volatile sig_atomic_t start_process    = 1;
static volatile sig_atomic_t graceful_restart = 0;
static volatile pid_t pid = -1;

#define UNUSED(x) ( (void)(x) )

static void sigaction_handler(int sig, siginfo_t *si, void *context) {
	int exitcode;

	UNUSED(context);
	switch (sig) {
	case SIGINT: 
	case SIGTERM:
		memcpy(&last_sigterm_info, si, sizeof(*si));

		/** forward the sig to the child */
		kill(pid, sig);
		break;
	case SIGHUP: /** do a graceful restart */
		memcpy(&last_sighup_info, si, sizeof(*si));

		/** do a graceful shutdown on the main process and start a new child */
		kill(pid, SIGINT);

		usleep(5 * 1000); /** wait 5 microsec */
		
		start_process = 1;
		break;
	case SIGCHLD:
		/** a child died, de-combie it */
		wait(&exitcode);
		break;
	}
}

int main(int argc, char **argv) {
	int is_shutdown = 0;
	struct sigaction act;

	UNUSED(argc);

	/**
	 * we are running as root BEWARE
	 */

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);

	act.sa_sigaction = sigaction_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);

	/* check that the compiled in path has the right user,
	 *
	 * BEWARE: there is a race between the check here and the exec later
	 */

	while (!is_shutdown) {
		int exitcode = 0;

		if (start_process) {
			pid = fork();

			if (0 == pid) {
				/* i'm the child */

				argv[0] = BINPATH;

				execvp(BINPATH, argv);

				exit(1);
			} else if (-1 == pid) {
				/** error */

				return -1;
			}

			/* I'm the angel */
			start_process = 0;
		}
	       
		if ((pid_t)-1 == waitpid(pid, &exitcode, 0)) {
			switch (errno) {
			case EINTR:
				/* someone sent a signal ... 
				 * do we have to shutdown or restart the process */
				break;
			case ECHILD:
				/** 
				 * make sure we are not in a race between the signal handler
				 * and the process restart */
				if (!start_process) is_shutdown = 1;
				break;
			default:
				break;
			}
		} else {
			/** process went away */

			if (WIFEXITED(exitcode)) {
				/** normal exit */

				is_shutdown = 1;

				fprintf(stderr, "%s.%d: child (pid=%d) exited normally with exitcode: %d\n", 
						__FILE__, __LINE__,
						pid,
						WEXITSTATUS(exitcode));

			} else if (WIFSIGNALED(exitcode)) {
				/** got a signal */

				fprintf(stderr, "%s.%d: child (pid=%d) exited unexpectedly with signal %d, restarting\n", 
						__FILE__, __LINE__,
						pid,
						WTERMSIG(exitcode));

				start_process = 1;
			}
		}
	}

	return 0;
}

