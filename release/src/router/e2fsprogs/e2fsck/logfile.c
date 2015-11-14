/*
 * logfile.c --- set up e2fsck log files
 *
 * Copyright 1996, 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "e2fsck.h"
#include <pwd.h>

extern e2fsck_t e2fsck_global_ctx;   /* Try your very best not to use this! */

struct string {
	char	*s;
	int	len;
	int	end;
};

static void alloc_string(struct string *s, int len)
{
	s->s = malloc(len);
/* e2fsck_allocate_memory(ctx, len, "logfile name"); */
	s->len = len;
	s->end = 0;
}

static void append_string(struct string *s, const char *a, int len)
{
	int needlen;

	if (!len)
		len = strlen(a);

	needlen = s->end + len + 1;
	if (needlen > s->len) {
		char *n;

		if (s->len * 2 > needlen)
			needlen = s->len * 2;
	        n = realloc(s->s, needlen);

		if (n) {
			s->s = n;
			s->len = needlen;
		} else {
			/* Don't append if we ran out of memory */
			return;
		}
	}
	memcpy(s->s + s->end, a, len);
	s->end += len;
	s->s[s->end] = 0;
}

#define FLAG_UTC	0x0001

static void expand_percent_expression(e2fsck_t ctx, char ch,
				      struct string *s, int *flags)
{
	struct tm	*tm = NULL, tm_struct;
	struct passwd	*pw = NULL, pw_struct;
	char		*cp;
	char		buf[256];

	if ((ch == 'D') || (ch == 'd') || (ch == 'm') || (ch == 'y') ||
	    (ch == 'Y') ||
	    (ch == 'T') || (ch == 'H') || (ch == 'M') || (ch == 'S')) {
		tzset();
		tm = (*flags & FLAG_UTC) ? gmtime_r(&ctx->now, &tm_struct) :
			localtime_r(&ctx->now, &tm_struct);
	}

	switch (ch) {
	case '%':
		append_string(s, "%", 1);
		return;
	case 'd':
		sprintf(buf, "%02d", tm->tm_mday);
		break;
	case 'D':
		sprintf(buf, "%d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1,
			tm->tm_mday);
		break;
	case 'h':
#ifdef TEST_PROGRAM
		strcpy(buf, "server");
#else
		buf[0] = 0;
		gethostname(buf, sizeof(buf));
		buf[sizeof(buf)-1] = 0;
#endif
		break;
	case 'H':
		sprintf(buf, "%02d", tm->tm_hour);
		break;
	case 'm':
		sprintf(buf, "%02d", tm->tm_mon + 1);
		break;
	case 'M':
		sprintf(buf, "%02d", tm->tm_min);
		break;
	case 'N':		/* block device name */
		cp = strrchr(ctx->filesystem_name, '/');
		if (cp)
			cp++;
		else
			cp = ctx->filesystem_name;
		append_string(s, cp, 0);
		return;
	case 'p':
		sprintf(buf, "%lu", (unsigned long) getpid());
		break;
	case 's':
		sprintf(buf, "%lu", (unsigned long) ctx->now);
		break;
	case 'S':
		sprintf(buf, "%02d", tm->tm_sec);
		break;
	case 'T':
		sprintf(buf, "%02d%02d%02d", tm->tm_hour, tm->tm_min,
			tm->tm_sec);
		break;
	case 'u':
#ifdef TEST_PROGRAM
		strcpy(buf, "tytso");
		break;
#else
#ifdef HAVE_GETPWUID_R
		getpwuid_r(getuid(), &pw_struct, buf, sizeof(buf), &pw);
#else
		pw = getpwuid(getuid());
#endif
		if (pw)
			append_string(s, pw->pw_name, 0);
		return;
#endif
	case 'U':
		*flags |= FLAG_UTC;
		return;
	case 'y':
		sprintf(buf, "%02d", tm->tm_year % 100);
		break;
	case 'Y':
		sprintf(buf, "%d", tm->tm_year + 1900);
		break;
	}
	append_string(s, buf, 0);
}

static void expand_logfn(e2fsck_t ctx, const char *log_fn, struct string *s)
{
	const char	*cp;
	int		i;
	int		flags = 0;

	alloc_string(s, 100);
	for (cp = log_fn; *cp; cp++) {
		if (cp[0] == '%') {
			cp++;
			expand_percent_expression(ctx, *cp, s, &flags);
			continue;
		}
		for (i = 0; cp[i]; i++)
			if (cp[i] == '%')
				break;
		append_string(s, cp, i);
		cp += i-1;
	}
}

static int	outbufsize;
static void	*outbuf;

static int do_read(int fd)
{
	int	c;
	char		*n;
	char	buffer[4096];

	c = read(fd, buffer, sizeof(buffer)-1);
	if (c <= 0)
		return c;

	n = realloc(outbuf, outbufsize + c);
	if (n) {
		outbuf = n;
		memcpy(((char *)outbuf)+outbufsize, buffer, c);
		outbufsize += c;
	}
	return c;
}

/*
 * Fork a child process to save the output of the logfile until the
 * appropriate file system is mounted read/write.
 */
static FILE *save_output(const char *s0, const char *s1, const char *s2)
{
	int c, fd, fds[2];
	char *cp;
	pid_t pid;
	FILE *ret;

	if (s0 && *s0 == 0)
		s0 = 0;
	if (s1 && *s1 == 0)
		s1 = 0;
	if (s2 && *s2 == 0)
		s2 = 0;

	/* At least one potential output file name is valid */
	if (!s0 && !s1 && !s2)
		return NULL;
	if (pipe(fds) < 0) {
		perror("pipe");
		exit(1);
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) {
		if (e2fsck_global_ctx && e2fsck_global_ctx->progress_fd)
			close(e2fsck_global_ctx->progress_fd);
		if (daemon(0, 0) < 0) {
			perror("daemon");
			exit(1);
		}
		/*
		 * Grab the output from our parent
		 */
		close(fds[1]);
		while (do_read(fds[0]) > 0)
			;
		close(fds[0]);

		/* OK, now let's try to open the output file */
		fd = -1;
		while (1) {
			if (fd < 0 && s0)
				fd = open(s0, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (fd < 0 && s1)
				fd = open(s1, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (fd < 0 && s2)
				fd = open(s2, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (fd >= 0)
				break;
			sleep(1);
		}

		cp = outbuf;
		while (outbufsize > 0) {
			c = write(fd, cp, outbufsize);
			if (c < 0) {
				if ((errno == EAGAIN) || (errno == EINTR))
					continue;
				break;
			}
			outbufsize -= c;
			cp += c;
		}
		exit(0);
	}

	close(fds[0]);
	ret = fdopen(fds[1], "w");
	if (!ret)
		close(fds[1]);
	return ret;
}

#ifndef TEST_PROGRAM
void set_up_logging(e2fsck_t ctx)
{
	struct string s, s1, s2;
	char *s0 = 0, *log_dir = 0, *log_fn = 0;
	int log_dir_wait = 0;

	s.s = s1.s = s2.s = 0;

	profile_get_boolean(ctx->profile, "options", "log_dir_wait", 0, 0,
			    &log_dir_wait);
	if (ctx->log_fn)
		log_fn = string_copy(ctx, ctx->log_fn, 0);
	else
		profile_get_string(ctx->profile, "options", "log_filename",
				   0, 0, &log_fn);
	profile_get_string(ctx->profile, "options", "log_dir", 0, 0, &log_dir);

	if (!log_fn || !log_fn[0])
		goto out;

	expand_logfn(ctx, log_fn, &s);
	if ((log_fn[0] == '/') || !log_dir || !log_dir[0])
		s0 = s.s;

	if (log_dir && log_dir[0]) {
		alloc_string(&s1, strlen(log_dir) + strlen(s.s) + 2);
		append_string(&s1, log_dir, 0);
		append_string(&s1, "/", 1);
		append_string(&s1, s.s, 0);
	}

	free(log_dir);
	profile_get_string(ctx->profile, "options", "log_dir_fallback", 0, 0,
			   &log_dir);
	if (log_dir && log_dir[0]) {
		alloc_string(&s2, strlen(log_dir) + strlen(s.s) + 2);
		append_string(&s2, log_dir, 0);
		append_string(&s2, "/", 1);
		append_string(&s2, s.s, 0);
		printf("%s\n", s2.s);
	}

	if (s0)
		ctx->logf = fopen(s0, "w");
	if (!ctx->logf && s1.s)
		ctx->logf = fopen(s1.s, "w");
	if (!ctx->logf && s2.s)
		ctx->logf = fopen(s2.s, "w");
	if (!ctx->logf && log_dir_wait)
		ctx->logf = save_output(s0, s1.s, s2.s);

out:
	free(s.s);
	free(s1.s);
	free(s2.s);
	free(log_fn);
	free(log_dir);
	return;
}
#else
void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
			     const char *description)
{
	void *ret;
	char buf[256];

	ret = malloc(size);
	if (!ret) {
		sprintf(buf, "Can't allocate %s\n", description);
		exit(1);
	}
	memset(ret, 0, size);
	return ret;
}

errcode_t e2fsck_allocate_context(e2fsck_t *ret)
{
	e2fsck_t	context;
	errcode_t	retval;
	char		*time_env;

	context = malloc(sizeof(struct e2fsck_struct));
	if (!context)
		return ENOMEM;

	memset(context, 0, sizeof(struct e2fsck_struct));

	context->now = 1332006474;

	context->filesystem_name = "/dev/sda3";
	context->device_name = "fslabel";

	*ret = context;
	return 0;
}

int main(int argc, char **argv)
{
	e2fsck_t	ctx;
	struct string	s;

	putenv("TZ=EST+5:00");
	e2fsck_allocate_context(&ctx);
	expand_logfn(ctx, "e2fsck-%N.%h.%u.%D-%T", &s);
	printf("%s\n", s.s);
	free(s.s);
	expand_logfn(ctx, "e2fsck-%N.%h.%u.%Y%m%d-%H%M%S", &s);
	printf("%s\n", s.s);
	free(s.s);

	return 0;
}
#endif
