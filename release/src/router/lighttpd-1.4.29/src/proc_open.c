#include "proc_open.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#ifdef WIN32
# include <io.h>
# include <fcntl.h>
#else
# include <sys/wait.h>
# include <unistd.h>
#endif


#ifdef WIN32
/* {{{ win32 stuff */
# define SHELLENV "ComSpec"
# define SECURITY_DC , SECURITY_ATTRIBUTES *security
# define SECURITY_CC , security
# define pipe(pair) (CreatePipe(&pair[0], &pair[1], security, 2048L) ? 0 : -1)
static inline HANDLE dup_handle(HANDLE src, BOOL inherit, BOOL closeorig)
{
	HANDLE copy, self = GetCurrentProcess();

	if (!DuplicateHandle(self, src, self, &copy, 0, inherit, DUPLICATE_SAME_ACCESS |
				(closeorig ? DUPLICATE_CLOSE_SOURCE : 0)))
		return NULL;
	return copy;
}
# define close_descriptor(fd) CloseHandle(fd)
static void pipe_close_parent(pipe_t *p) {
	/* don't let the child inherit the parent side of the pipe */
	p->parent = dup_handle(p->parent, FALSE, TRUE);
}
static void pipe_close_child(pipe_t *p) {
	close_descriptor(p->child);
	p->fd = _open_osfhandle((long)p->parent,
			(p->fd == 0 ? O_RDONLY : O_WRONLY)|O_BINARY);
}
/* }}} */
#else /* WIN32 */
/* {{{ unix way */
# define SHELLENV "SHELL"
# define SECURITY_DC
# define SECURITY_CC
# define close_descriptor(fd) close(fd)
static void pipe_close_parent(pipe_t *p) {
	/* don't close stdin */
	close_descriptor(p->parent);
	if (dup2(p->child, p->fd) != p->fd) {
		perror("pipe_child dup2");
	} else {
		close_descriptor(p->child);
		p->child = p->fd;
	}
}
static void pipe_close_child(pipe_t *p) {
	close_descriptor(p->child);
	p->fd = p->parent;
}
/* }}} */
#endif /* WIN32 */

/* {{{ pipe_close */
static void pipe_close(pipe_t *p) {
	close_descriptor(p->parent);
	close_descriptor(p->child);
#ifdef WIN32
	close(p->fd);
#endif
}
/* }}} */
/* {{{ pipe_open */
static int pipe_open(pipe_t *p, int fd SECURITY_DC) {
	descriptor_t newpipe[2];

	if (0 != pipe(newpipe)) {
		fprintf(stderr, "can't open pipe");
		return -1;
	}
	if (0 == fd) {
		p->parent = newpipe[1]; /* write */
		p->child  = newpipe[0]; /* read */
	} else {
		p->parent = newpipe[0]; /* read */
		p->child  = newpipe[1]; /* write */
	}
	p->fd = fd;

	return 0;
}
/* }}} */

/* {{{ proc_open_pipes */
static int proc_open_pipes(proc_handler_t *proc SECURITY_DC) {
	if (pipe_open(&(proc->in), 0 SECURITY_CC) != 0) {
		return -1;
	}
	if (pipe_open(&(proc->out), 1 SECURITY_CC) != 0) {
		return -1;
	}
	if (pipe_open(&(proc->err), 2 SECURITY_CC) != 0) {
		return -1;
	}
	return 0;
}
/* }}} */
/* {{{ proc_close_pipes */
static void proc_close_pipes(proc_handler_t *proc) {
	pipe_close(&proc->in);
	pipe_close(&proc->out);
	pipe_close(&proc->err);
}
/* }}} */
/* {{{ proc_close_parents */
static void proc_close_parents(proc_handler_t *proc) {
	pipe_close_parent(&proc->in);
	pipe_close_parent(&proc->out);
	pipe_close_parent(&proc->err);
}
/* }}} */
/* {{{ proc_close_childs */
static void proc_close_childs(proc_handler_t *proc) {
	pipe_close_child(&proc->in);
	pipe_close_child(&proc->out);
	pipe_close_child(&proc->err);
}
/* }}} */

#ifdef WIN32
/* {{{ proc_close */
int proc_close(proc_handler_t *proc) {
	proc_pid_t child = proc->child;
	DWORD wstatus;

	proc_close_pipes(proc);
	WaitForSingleObject(child, INFINITE);
	GetExitCodeProcess(child, &wstatus);
	CloseHandle(child);

	return wstatus;
}
/* }}} */
/* {{{ proc_open */
int proc_open(proc_handler_t *proc, const char *command) {
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL procok;
	SECURITY_ATTRIBUTES security;
	const char *shell = NULL;
	const char *windir = NULL;
	buffer *cmdline;

	if (NULL == (shell = getenv(SHELLENV)) &&
			NULL == (windir = getenv("SystemRoot")) &&
			NULL == (windir = getenv("windir"))) {
		fprintf(stderr, "One of %s,%%SystemRoot,%%windir is required", SHELLENV);
		return -1;
	}

	/* we use this to allow the child to inherit handles */
	memset(&security, 0, sizeof(security));
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;
	security.lpSecurityDescriptor = NULL;

	if (proc_open_pipes(proc, &security) != 0) {
		return -1;
	}
	proc_close_parents(proc);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = proc->in.child;
	si.hStdOutput = proc->out.child;
	si.hStdError = proc->err.child;

	memset(&pi, 0, sizeof(pi));

	cmdline = buffer_init();
	if (shell) {
		buffer_append_string(cmdline, shell);
	} else {
		buffer_append_string(cmdline, windir);
		buffer_append_string_len(cmdline, CONST_STR_LEN("\\system32\\cmd.exe"));
	}
	buffer_append_string_len(cmdline, CONST_STR_LEN(" /c "));
	buffer_append_string(cmdline, command);
	procok = CreateProcess(NULL, cmdline->ptr, &security, &security, TRUE,
			NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);

	if (FALSE == procok) {
		fprintf(stderr, "failed to CreateProcess: %s", cmdline->ptr);
		buffer_free(cmdline);
		return -1;
	}
	buffer_free(cmdline);

	proc->child = pi.hProcess;
	CloseHandle(pi.hThread);

	proc_close_childs(proc);

	return 0;
}
/* }}} */
#else /* WIN32 */
/* {{{ proc_close */
int proc_close(proc_handler_t *proc) {
	pid_t child = proc->child;
	int wstatus;
	pid_t wait_pid;

	proc_close_pipes(proc);

	do {
		wait_pid = waitpid(child, &wstatus, 0);
	} while (wait_pid == -1 && errno == EINTR);

	if (wait_pid == -1) {
		return -1;
	} else {
		if (WIFEXITED(wstatus))
			wstatus = WEXITSTATUS(wstatus);
	}

	return wstatus;
}
/* }}} */
/* {{{ proc_open */
int proc_open(proc_handler_t *proc, const char *command) {
	pid_t child;
	const char *shell;

	if (NULL == (shell = getenv(SHELLENV))) {
		shell = "/bin/sh";
	}

	if (proc_open_pipes(proc) != 0) {
		return -1;
	}

	/* the unix way */

	child = fork();

	if (child == 0) {
		/* this is the child process */

		/* close those descriptors that we just opened for the parent stuff,
		 * dup new descriptors into required descriptors and close the original
		 * cruft
		 */
		proc_close_parents(proc);

		execl(shell, shell, "-c", command, (char *)NULL);
		fprintf(stderr, "failed to execute shell: %s -c %s: %s\n", shell, command, strerror(errno));
		_exit(127);

	} else if (child < 0) {
		fprintf(stderr, "failed to forking");
		proc_close(proc);
		return -1;

	} else {
		proc->child = child;
		proc_close_childs(proc);
		return 0;
	}
}
/* }}} */
#endif /* WIN32 */

/* {{{ proc_read_fd_to_buffer */
static void proc_read_fd_to_buffer(int fd, buffer *b) {
	ssize_t s;

	for (;;) {
		buffer_prepare_append(b, 512);
		if ((s = read(fd, (void *)(b->ptr + b->used), 512 - 1)) <= 0) {
			break;
		}
		b->used += s;
	}
	b->ptr[b->used] = '\0';
}
/* }}} */
/* {{{ proc_open_buffer */
int proc_open_buffer(const char *command, buffer *in, buffer *out, buffer *err) {
	proc_handler_t proc;

	if (proc_open(&proc, command) != 0) {
		return -1;
	}

	if (in) {
		if (write(proc.in.fd, (void *)in->ptr, in->used) < 0) {
			perror("error writing pipe");
			return -1;
		}
	}
	pipe_close(&proc.in);

	if (out) {
		proc_read_fd_to_buffer(proc.out.fd, out);
	}
	pipe_close(&proc.out);

	if (err) {
		proc_read_fd_to_buffer(proc.err.fd, err);
	} else {
		buffer *tmp = buffer_init();
		proc_read_fd_to_buffer(proc.err.fd, tmp);
		if (tmp->used > 0 &&  write(2, (void*)tmp->ptr, tmp->used) < 0) {
			perror("error writing pipe");
			return -1;
		}
		buffer_free(tmp);
	}
	pipe_close(&proc.err);

	proc_close(&proc);

	return 0;
}
/* }}} */

/* {{{ test */
#ifdef DEBUG_PROC_OPEN
int main() {
	proc_handler_t proc;
	buffer *in = buffer_init(), *out = buffer_init(), *err = buffer_init();
	int wstatus;

#define FREE() do { \
	buffer_free(in); \
	buffer_free(out); \
	buffer_free(err); \
} while (0)

#define RESET() do { \
	buffer_reset(in); \
	buffer_reset(out); \
	buffer_reset(err); \
	wstatus = proc_close(&proc); \
	if (0&&wstatus != 0) { \
		fprintf(stdout, "exitstatus %d\n", wstatus); \
		return __LINE__ - 200; \
	} \
} while (0)

#define ERROR_OUT() do { \
	fprintf(stdout, "failed opening proc\n"); \
	wstatus = proc_close(&proc); \
	fprintf(stdout, "exitstatus %d\n", wstatus); \
	FREE(); \
	return __LINE__ - 300; \
} while (0)

#ifdef WIN32
#define CMD_CAT "pause"
#else
#define CMD_CAT "cat"
#endif

	do {
		fprintf(stdout, "test: echo 123 without read\n");
		if (proc_open(&proc, "echo 321") != 0) {
			ERROR_OUT();
		}
		close_descriptor(proc.in.parent);
		close_descriptor(proc.out.parent);
		close_descriptor(proc.err.parent);
		RESET();

		fprintf(stdout, "test: echo 321 with read\n"); fflush(stdout);
		if (proc_open_buffer("echo 321", NULL, out, err) != 0) {
			ERROR_OUT();
		}
		fprintf(stdout, "result: ->%s<-\n\n", out->ptr); fflush(stdout);
		RESET();

		fprintf(stdout, "test: echo 123 | " CMD_CAT "\n"); fflush(stdout);
		buffer_copy_string_len(in, CONST_STR_LEN("123\n"));
		if (proc_open_buffer(CMD_CAT, in, out, err) != 0) {
			ERROR_OUT();
		}
		fprintf(stdout, "result: ->%s<-\n\n", out->ptr); fflush(stdout);
		RESET();
	} while (0);

#undef RESET
#undef ERROR_OUT

	fprintf(stdout, "ok\n");

	FREE();
	return 0;
}
#endif /* DEBUG_PROC_OPEN */
/* }}} */

