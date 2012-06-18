/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

/* transformer(), more than meets the eye */
/*
 * On MMU machine, the transform_prog is removed by macro magic
 * in include/unarchive.h. On NOMMU, transformer is removed.
 */
void FAST_FUNC open_transformer(int fd,
	IF_DESKTOP(long long) int FAST_FUNC (*transformer)(int src_fd, int dst_fd),
	const char *transform_prog)
{
	struct fd_pair fd_pipe;
	int pid;

	xpiped_pair(fd_pipe);
	pid = BB_MMU ? xfork() : xvfork();
	if (pid == 0) {
		/* Child */
		close(fd_pipe.rd); /* we don't want to read from the parent */
		// FIXME: error check?
#if BB_MMU
		transformer(fd, fd_pipe.wr);
		if (ENABLE_FEATURE_CLEAN_UP) {
			close(fd_pipe.wr); /* send EOF */
			close(fd);
		}
		/* must be _exit! bug was actually seen here */
		_exit(EXIT_SUCCESS);
#else
		{
			char *argv[4];
			xmove_fd(fd, 0);
			xmove_fd(fd_pipe.wr, 1);
			argv[0] = (char*)transform_prog;
			argv[1] = (char*)"-cf";
			argv[2] = (char*)"-";
			argv[3] = NULL;
			BB_EXECVP(transform_prog, argv);
			bb_perror_msg_and_die("can't execute '%s'", transform_prog);
		}
#endif
		/* notreached */
	}

	/* parent process */
	close(fd_pipe.wr); /* don't want to write to the child */
	xmove_fd(fd_pipe.rd, fd);
}
