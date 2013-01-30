/* vi: set sw=4 ts=4: */
/*
 * helper routines
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "mail.h"

static void kill_helper(void)
{
	if (G.helper_pid > 0) {
		kill(G.helper_pid, SIGTERM);
		G.helper_pid = 0;
	}
}

// generic signal handler
static void signal_handler(int signo)
{
#define err signo
	if (SIGALRM == signo) {
		kill_helper();
		bb_error_msg_and_die("timed out");
	}

	// SIGCHLD. reap zombies
	if (safe_waitpid(G.helper_pid, &err, WNOHANG) > 0) {
		if (WIFSIGNALED(err))
			bb_error_msg_and_die("helper killed by signal %u", WTERMSIG(err));
		if (WIFEXITED(err)) {
			G.helper_pid = 0;
			if (WEXITSTATUS(err))
				bb_error_msg_and_die("helper exited (%u)", WEXITSTATUS(err));
		}
	}
#undef err
}

void FAST_FUNC launch_helper(const char **argv)
{
	// setup vanilla unidirectional pipes interchange
	int i;
	int pipes[4];

	xpipe(pipes);
	xpipe(pipes + 2);

	// NB: handler must be installed before vfork
	bb_signals(0
		+ (1 << SIGCHLD)
		+ (1 << SIGALRM)
		, signal_handler);

	G.helper_pid = xvfork();

	i = (!G.helper_pid) * 2; // for parent:0, for child:2
	close(pipes[i + 1]); // 1 or 3 - closing one write end
	close(pipes[2 - i]); // 2 or 0 - closing one read end
	xmove_fd(pipes[i], STDIN_FILENO); // 0 or 2 - using other read end
	xmove_fd(pipes[3 - i], STDOUT_FILENO); // 3 or 1 - other write end

	if (!G.helper_pid) {
		// child: try to execute connection helper
		// NB: SIGCHLD & SIGALRM revert to SIG_DFL on exec
		BB_EXECVP_or_die((char**)argv);
	}

	// parent
	// check whether child is alive
	//redundant:signal_handler(SIGCHLD);
	// child seems OK -> parent goes on
	atexit(kill_helper);
}

const FAST_FUNC char *command(const char *fmt, const char *param)
{
	const char *msg = fmt;
	if (timeout)
		alarm(timeout);
	if (msg) {
		msg = xasprintf(fmt, param);
		printf("%s\r\n", msg);
	}
	fflush_all();
	return msg;
}

// NB: parse_url can modify url[] (despite const), but only if '@' is there
/*
static char FAST_FUNC *parse_url(char *url, char **user, char **pass)
{
	// parse [user[:pass]@]host
	// return host
	char *s = strchr(url, '@');
	*user = *pass = NULL;
	if (s) {
		*s++ = '\0';
		*user = url;
		url = s;
		s = strchr(*user, ':');
		if (s) {
			*s++ = '\0';
			*pass = s;
		}
	}
	return url;
}
*/

void FAST_FUNC encode_base64(char *fname, const char *text, const char *eol)
{
	enum {
		SRC_BUF_SIZE = 45,  /* This *MUST* be a multiple of 3 */
		DST_BUF_SIZE = 4 * ((SRC_BUF_SIZE + 2) / 3),
	};

#define src_buf text
	FILE *fp = fp;
	ssize_t len = len;
	char dst_buf[DST_BUF_SIZE + 1];

	if (fname) {
		fp = (NOT_LONE_DASH(fname)) ? xfopen_for_read(fname) : (FILE *)text;
		src_buf = bb_common_bufsiz1;
	// N.B. strlen(NULL) segfaults!
	} else if (text) {
		// though we do not call uuencode(NULL, NULL) explicitly
		// still we do not want to break things suddenly
		len = strlen(text);
	} else
		return;

	while (1) {
		size_t size;
		if (fname) {
			size = fread((char *)src_buf, 1, SRC_BUF_SIZE, fp);
			if ((ssize_t)size < 0)
				bb_perror_msg_and_die(bb_msg_read_error);
		} else {
			size = len;
			if (len > SRC_BUF_SIZE)
				size = SRC_BUF_SIZE;
		}
		if (!size)
			break;
		// encode the buffer we just read in
		bb_uuencode(dst_buf, src_buf, size, bb_uuenc_tbl_base64);
		if (fname) {
			printf("%s\n", eol);
		} else {
			src_buf += size;
			len -= size;
		}
		fwrite(dst_buf, 1, 4 * ((size + 2) / 3), stdout);
	}
	if (fname && NOT_LONE_DASH(fname))
		fclose(fp);
#undef src_buf
}

void FAST_FUNC decode_base64(FILE *src_stream, FILE *dst_stream)
{
	int term_count = 1;

	while (1) {
		char translated[4];
		int count = 0;

		while (count < 4) {
			char *table_ptr;
			int ch;

			/* Get next _valid_ character.
			 * global vector bb_uuenc_tbl_base64[] contains this string:
			 * "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n"
			 */
			do {
				ch = fgetc(src_stream);
				if (ch == EOF) {
					bb_error_msg_and_die(bb_msg_read_error);
				}
				// - means end of MIME section
				if ('-' == ch) {
					// push it back
					ungetc(ch, src_stream);
					return;
				}
				table_ptr = strchr(bb_uuenc_tbl_base64, ch);
			} while (table_ptr == NULL);

			/* Convert encoded character to decimal */
			ch = table_ptr - bb_uuenc_tbl_base64;

			if (*table_ptr == '=') {
				if (term_count == 0) {
					translated[count] = '\0';
					break;
				}
				term_count++;
			} else if (*table_ptr == '\n') {
				/* Check for terminating line */
				if (term_count == 5) {
					return;
				}
				term_count = 1;
				continue;
			} else {
				translated[count] = ch;
				count++;
				term_count = 0;
			}
		}

		/* Merge 6 bit chars to 8 bit */
		if (count > 1) {
			fputc(translated[0] << 2 | translated[1] >> 4, dst_stream);
		}
		if (count > 2) {
			fputc(translated[1] << 4 | translated[2] >> 2, dst_stream);
		}
		if (count > 3) {
			fputc(translated[2] << 6 | translated[3], dst_stream);
		}
	}
}


/*
 * get username and password from a file descriptor
 */
void FAST_FUNC get_cred_or_die(int fd)
{
	if (isatty(fd)) {
		G.user = xstrdup(bb_ask(fd, /* timeout: */ 0, "User: "));
		G.pass = xstrdup(bb_ask(fd, /* timeout: */ 0, "Password: "));
	} else {
		G.user = xmalloc_reads(fd, /* pfx: */ NULL, /* maxsize: */ NULL);
		G.pass = xmalloc_reads(fd, /* pfx: */ NULL, /* maxsize: */ NULL);
	}
	if (!G.user || !*G.user || !G.pass)
		bb_error_msg_and_die("no username or password");
}
