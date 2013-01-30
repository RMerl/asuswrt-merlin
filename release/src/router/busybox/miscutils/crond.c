/* vi: set sw=4 ts=4: */
/*
 * crond -d[#] -c <crondir> -f -b
 *
 * run as root, but NOT setuid root
 *
 * Copyright 1994 Matthew Dillon (dillon@apollo.west.oic.com)
 * (version 2.3.2)
 * Vladimir Oleynik <dzo@simtreas.ru> (C) 2002
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"
#include <syslog.h>

/* glibc frees previous setenv'ed value when we do next setenv()
 * of the same variable. uclibc does not do this! */
#if (defined(__GLIBC__) && !defined(__UCLIBC__)) /* || OTHER_SAFE_LIBC... */
#define SETENV_LEAKS 0
#else
#define SETENV_LEAKS 1
#endif


#define TMPDIR          CONFIG_FEATURE_CROND_DIR
#define CRONTABS        CONFIG_FEATURE_CROND_DIR "/crontabs"
#ifndef SENDMAIL
#define SENDMAIL        "sendmail"
#endif
#ifndef SENDMAIL_ARGS
#define SENDMAIL_ARGS   "-ti", NULL
#endif
#ifndef CRONUPDATE
#define CRONUPDATE      "cron.update"
#endif
#ifndef MAXLINES
#define MAXLINES        256	/* max lines in non-root crontabs */
#endif


typedef struct CronFile {
	struct CronFile *cf_Next;
	struct CronLine *cf_LineBase;
	char *cf_User;                  /* username                     */
	smallint cf_Ready;              /* bool: one or more jobs ready */
	smallint cf_Running;            /* bool: one or more jobs running */
	smallint cf_Deleted;            /* marked for deletion, ignore  */
} CronFile;

typedef struct CronLine {
	struct CronLine *cl_Next;
	char *cl_Shell;         /* shell command                        */
	pid_t cl_Pid;           /* running pid, 0, or armed (-1)        */
#if ENABLE_FEATURE_CROND_CALL_SENDMAIL
	int cl_MailPos;         /* 'empty file' size                    */
	smallint cl_MailFlag;   /* running pid is for mail              */
	char *cl_MailTo;	/* whom to mail results                 */
#endif
	/* ordered by size, not in natural order. makes code smaller: */
	char cl_Dow[7];         /* 0-6, beginning sunday                */
	char cl_Mons[12];       /* 0-11                                 */
	char cl_Hrs[24];        /* 0-23                                 */
	char cl_Days[32];       /* 1-31                                 */
	char cl_Mins[60];       /* 0-59                                 */
} CronLine;


#define DaemonUid 0


enum {
	OPT_l = (1 << 0),
	OPT_L = (1 << 1),
	OPT_f = (1 << 2),
	OPT_b = (1 << 3),
	OPT_S = (1 << 4),
	OPT_c = (1 << 5),
	OPT_d = (1 << 6) * ENABLE_FEATURE_CROND_D,
};
#if ENABLE_FEATURE_CROND_D
#define DebugOpt (option_mask32 & OPT_d)
#else
#define DebugOpt 0
#endif


struct globals {
	unsigned LogLevel; /* = 8; */
	const char *LogFile;
	const char *CDir; /* = CRONTABS; */
	CronFile *FileBase;
#if SETENV_LEAKS
	char *env_var_user;
	char *env_var_home;
#endif
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define LogLevel           (G.LogLevel               )
#define LogFile            (G.LogFile                )
#define CDir               (G.CDir                   )
#define FileBase           (G.FileBase               )
#define env_var_user       (G.env_var_user           )
#define env_var_home       (G.env_var_home           )
#define INIT_G() do { \
	LogLevel = 8; \
	CDir = CRONTABS; \
} while (0)


static void CheckUpdates(void);
static void SynchronizeDir(void);
static int TestJobs(time_t t1, time_t t2);
static void RunJobs(void);
static int CheckJobs(void);
static void RunJob(const char *user, CronLine *line);
#if ENABLE_FEATURE_CROND_CALL_SENDMAIL
static void EndJob(const char *user, CronLine *line);
#else
#define EndJob(user, line)  ((line)->cl_Pid = 0)
#endif
static void DeleteFile(const char *userName);


/* 0 is the most verbose, default 8 */
#define LVL5  "\x05"
#define LVL7  "\x07"
#define LVL8  "\x08"
#define WARN9 "\x49"
#define DIE9  "\xc9"
/* level >= 20 is "error" */
#define ERR20 "\x14"

static void crondlog(const char *ctl, ...) __attribute__ ((format (printf, 1, 2)));
static void crondlog(const char *ctl, ...)
{
	va_list va;
	int level = (ctl[0] & 0x1f);

	va_start(va, ctl);
	if (level >= (int)LogLevel) {
		/* Debug mode: all to (non-redirected) stderr, */
		/* Syslog mode: all to syslog (logmode = LOGMODE_SYSLOG), */
		if (!DebugOpt && LogFile) {
			/* Otherwise (log to file): we reopen log file at every write: */
			int logfd = open3_or_warn(LogFile, O_WRONLY | O_CREAT | O_APPEND, 0666);
			if (logfd >= 0)
				xmove_fd(logfd, STDERR_FILENO);
		}
		/* When we log to syslog, level > 8 is logged at LOG_ERR
		 * syslog level, level <= 8 is logged at LOG_INFO. */
		if (level > 8) {
			bb_verror_msg(ctl + 1, va, /* strerr: */ NULL);
		} else {
			char *msg = NULL;
			vasprintf(&msg, ctl + 1, va);
			bb_info_msg("%s: %s", applet_name, msg);
			free(msg);
		}
	}
	va_end(va);
	if (ctl[0] & 0x80)
		exit(20);
}

int crond_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int crond_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opts;

	INIT_G();

	/* "-b after -f is ignored", and so on for every pair a-b */
	opt_complementary = "f-b:b-f:S-L:L-S" IF_FEATURE_CROND_D(":d-l")
			":l+:d+"; /* -l and -d have numeric param */
	opts = getopt32(argv, "l:L:fbSc:" IF_FEATURE_CROND_D("d:"),
			&LogLevel, &LogFile, &CDir
			IF_FEATURE_CROND_D(,&LogLevel));
	/* both -d N and -l N set the same variable: LogLevel */

	if (!(opts & OPT_f)) {
		/* close stdin, stdout, stderr.
		 * close unused descriptors - don't need them. */
		bb_daemonize_or_rexec(DAEMON_CLOSE_EXTRA_FDS, argv);
	}

	if (!(opts & OPT_d) && LogFile == NULL) {
		/* logging to syslog */
		openlog(applet_name, LOG_CONS | LOG_PID, LOG_CRON);
		logmode = LOGMODE_SYSLOG;
	}

	xchdir(CDir);
	//signal(SIGHUP, SIG_IGN); /* ? original crond dies on HUP... */
	xsetenv("SHELL", DEFAULT_SHELL); /* once, for all future children */
	crondlog(LVL8 "crond (busybox "BB_VER") started, log level %d", LogLevel);
	SynchronizeDir();

	/* main loop - synchronize to 1 second after the minute, minimum sleep
	 * of 1 second. */
	{
		time_t t1 = time(NULL);
		int rescan = 60;
		int sleep_time = 60;

		write_pidfile("/var/run/crond.pid");
		for (;;) {
			time_t t2;
			long dt;

			sleep((sleep_time + 1) - (time(NULL) % sleep_time));

			t2 = time(NULL);
			dt = (long)t2 - (long)t1;

			/*
			 * The file 'cron.update' is checked to determine new cron
			 * jobs.  The directory is rescanned once an hour to deal
			 * with any screwups.
			 *
			 * check for disparity.  Disparities over an hour either way
			 * result in resynchronization.  A reverse-indexed disparity
			 * less then an hour causes us to effectively sleep until we
			 * match the original time (i.e. no re-execution of jobs that
			 * have just been run).  A forward-indexed disparity less then
			 * an hour causes intermediate jobs to be run, but only once
			 * in the worst case.
			 *
			 * when running jobs, the inequality used is greater but not
			 * equal to t1, and less then or equal to t2.
			 */
			if (--rescan == 0) {
				rescan = 60;
				SynchronizeDir();
			}
			CheckUpdates();
			if (DebugOpt)
				crondlog(LVL5 "wakeup dt=%ld", dt);
			if (dt < -60 * 60 || dt > 60 * 60) {
				crondlog(WARN9 "time disparity of %ld minutes detected", dt / 60);
			} else if (dt > 0) {
				TestJobs(t1, t2);
				RunJobs();
				sleep(5);
				if (CheckJobs() > 0) {
					sleep_time = 10;
				} else {
					sleep_time = 60;
				}
			}
			t1 = t2;
		} /* for (;;) */
	}

	return 0; /* not reached */
}

#if SETENV_LEAKS
/* We set environment *before* vfork (because we want to use vfork),
 * so we cannot use setenv() - repeated calls to setenv() may leak memory!
 * Using putenv(), and freeing memory after unsetenv() won't leak */
static void safe_setenv(char **pvar_val, const char *var, const char *val)
{
	char *var_val = *pvar_val;

	if (var_val) {
		bb_unsetenv_and_free(var_val);
	}
	*pvar_val = xasprintf("%s=%s", var, val);
	putenv(*pvar_val);
}
#endif

static void SetEnv(struct passwd *pas)
{
#if SETENV_LEAKS
	safe_setenv(&env_var_user, "USER", pas->pw_name);
	safe_setenv(&env_var_home, "HOME", pas->pw_dir);
	/* if we want to set user's shell instead: */
	/*safe_setenv(env_var_user, "SHELL", pas->pw_shell);*/
#else
	xsetenv("USER", pas->pw_name);
	xsetenv("HOME", pas->pw_dir);
#endif
	/* currently, we use constant one: */
	/*setenv("SHELL", DEFAULT_SHELL, 1); - done earlier */
}

static void ChangeUser(struct passwd *pas)
{
	/* careful: we're after vfork! */
	change_identity(pas); /* - initgroups, setgid, setuid */
	if (chdir(pas->pw_dir) < 0) {
		crondlog(WARN9 "chdir(%s)", pas->pw_dir);
		if (chdir(TMPDIR) < 0) {
			crondlog(DIE9 "chdir(%s)", TMPDIR); /* exits */
		}
	}
}

static const char DowAry[] ALIGN1 =
	"sun""mon""tue""wed""thu""fri""sat"
	/* "Sun""Mon""Tue""Wed""Thu""Fri""Sat" */
;

static const char MonAry[] ALIGN1 =
	"jan""feb""mar""apr""may""jun""jul""aug""sep""oct""nov""dec"
	/* "Jan""Feb""Mar""Apr""May""Jun""Jul""Aug""Sep""Oct""Nov""Dec" */
;

static void ParseField(char *user, char *ary, int modvalue, int off,
				const char *names, char *ptr)
/* 'names' is a pointer to a set of 3-char abbreviations */
{
	char *base = ptr;
	int n1 = -1;
	int n2 = -1;

	// this can't happen due to config_read()
	/*if (base == NULL)
		return;*/

	while (1) {
		int skip = 0;

		/* Handle numeric digit or symbol or '*' */
		if (*ptr == '*') {
			n1 = 0;		/* everything will be filled */
			n2 = modvalue - 1;
			skip = 1;
			++ptr;
		} else if (isdigit(*ptr)) {
			char *endp;
			if (n1 < 0) {
				n1 = strtol(ptr, &endp, 10) + off;
			} else {
				n2 = strtol(ptr, &endp, 10) + off;
			}
			ptr = endp; /* gcc likes temp var for &endp */
			skip = 1;
		} else if (names) {
			int i;

			for (i = 0; names[i]; i += 3) {
				/* was using strncmp before... */
				if (strncasecmp(ptr, &names[i], 3) == 0) {
					ptr += 3;
					if (n1 < 0) {
						n1 = i / 3;
					} else {
						n2 = i / 3;
					}
					skip = 1;
					break;
				}
			}
		}

		/* handle optional range '-' */
		if (skip == 0) {
			goto err;
		}
		if (*ptr == '-' && n2 < 0) {
			++ptr;
			continue;
		}

		/*
		 * collapse single-value ranges, handle skipmark, and fill
		 * in the character array appropriately.
		 */
		if (n2 < 0) {
			n2 = n1;
		}
		if (*ptr == '/') {
			char *endp;
			skip = strtol(ptr + 1, &endp, 10);
			ptr = endp; /* gcc likes temp var for &endp */
		}

		/*
		 * fill array, using a failsafe is the easiest way to prevent
		 * an endless loop
		 */
		{
			int s0 = 1;
			int failsafe = 1024;

			--n1;
			do {
				n1 = (n1 + 1) % modvalue;

				if (--s0 == 0) {
					ary[n1 % modvalue] = 1;
					s0 = skip;
				}
				if (--failsafe == 0) {
					goto err;
				}
			} while (n1 != n2);

		}
		if (*ptr != ',') {
			break;
		}
		++ptr;
		n1 = -1;
		n2 = -1;
	}

	if (*ptr) {
 err:
		crondlog(WARN9 "user %s: parse error at %s", user, base);
		return;
	}

	if (DebugOpt && (LogLevel <= 5)) { /* like LVL5 */
		/* can't use crondlog, it inserts '\n' */
		int i;
		for (i = 0; i < modvalue; ++i)
			fprintf(stderr, "%d", (unsigned char)ary[i]);
		bb_putchar_stderr('\n');
	}
}

static void FixDayDow(CronLine *line)
{
	unsigned i;
	int weekUsed = 0;
	int daysUsed = 0;

	for (i = 0; i < ARRAY_SIZE(line->cl_Dow); ++i) {
		if (line->cl_Dow[i] == 0) {
			weekUsed = 1;
			break;
		}
	}
	for (i = 0; i < ARRAY_SIZE(line->cl_Days); ++i) {
		if (line->cl_Days[i] == 0) {
			daysUsed = 1;
			break;
		}
	}
	if (weekUsed != daysUsed) {
		if (weekUsed)
			memset(line->cl_Days, 0, sizeof(line->cl_Days));
		else /* daysUsed */
			memset(line->cl_Dow, 0, sizeof(line->cl_Dow));
	}
}

static void SynchronizeFile(const char *fileName)
{
	struct parser_t *parser;
	struct stat sbuf;
	int maxLines;
	char *tokens[6];
#if ENABLE_FEATURE_CROND_CALL_SENDMAIL
	char *mailTo = NULL;
#endif

	if (!fileName)
		return;

	DeleteFile(fileName);
	parser = config_open(fileName);
	if (!parser)
		return;

	maxLines = (strcmp(fileName, "root") == 0) ? 65535 : MAXLINES;

	if (fstat(fileno(parser->fp), &sbuf) == 0 && sbuf.st_uid == DaemonUid) {
		CronFile *file = xzalloc(sizeof(CronFile));
		CronLine **pline;
		int n;

		file->cf_User = xstrdup(fileName);
		pline = &file->cf_LineBase;

		while (1) {
			CronLine *line;

			if (!--maxLines)
				break;
			n = config_read(parser, tokens, 6, 1, "# \t", PARSE_NORMAL | PARSE_KEEP_COPY);
			if (!n)
				break;

			if (DebugOpt)
				crondlog(LVL5 "user:%s entry:%s", fileName, parser->data);

			/* check if line is setting MAILTO= */
			if (0 == strncmp(tokens[0], "MAILTO=", 7)) {
#if ENABLE_FEATURE_CROND_CALL_SENDMAIL
				free(mailTo);
				mailTo = (tokens[0][7]) ? xstrdup(&tokens[0][7]) : NULL;
#endif /* otherwise just ignore such lines */
				continue;
			}
			/* check if a minimum of tokens is specified */
			if (n < 6)
				continue;
			*pline = line = xzalloc(sizeof(*line));
			/* parse date ranges */
			ParseField(file->cf_User, line->cl_Mins, 60, 0, NULL, tokens[0]);
			ParseField(file->cf_User, line->cl_Hrs, 24, 0, NULL, tokens[1]);
			ParseField(file->cf_User, line->cl_Days, 32, 0, NULL, tokens[2]);
			ParseField(file->cf_User, line->cl_Mons, 12, -1, MonAry, tokens[3]);
			ParseField(file->cf_User, line->cl_Dow, 7, 0, DowAry, tokens[4]);
			/*
			 * fix days and dow - if one is not "*" and the other
			 * is "*", the other is set to 0, and vise-versa
			 */
			FixDayDow(line);
#if ENABLE_FEATURE_CROND_CALL_SENDMAIL
			/* copy mailto (can be NULL) */
			line->cl_MailTo = xstrdup(mailTo);
#endif
			/* copy command */
			line->cl_Shell = xstrdup(tokens[5]);
			if (DebugOpt) {
				crondlog(LVL5 " command:%s", tokens[5]);
			}
			pline = &line->cl_Next;
//bb_error_msg("M[%s]F[%s][%s][%s][%s][%s][%s]", mailTo, tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);
		}
		*pline = NULL;

		file->cf_Next = FileBase;
		FileBase = file;

		if (maxLines == 0) {
			crondlog(WARN9 "user %s: too many lines", fileName);
		}
	}
	config_close(parser);
}

static void CheckUpdates(void)
{
	FILE *fi;
	char buf[256];

	fi = fopen_for_read(CRONUPDATE);
	if (fi != NULL) {
		unlink(CRONUPDATE);
		while (fgets(buf, sizeof(buf), fi) != NULL) {
			/* use first word only */
			SynchronizeFile(strtok(buf, " \t\r\n"));
		}
		fclose(fi);
	}
}

static void SynchronizeDir(void)
{
	CronFile *file;
	/* Attempt to delete the database. */
 again:
	for (file = FileBase; file; file = file->cf_Next) {
		if (!file->cf_Deleted) {
			DeleteFile(file->cf_User);
			goto again;
		}
	}

	/*
	 * Remove cron update file
	 *
	 * Re-chdir, in case directory was renamed & deleted, or otherwise
	 * screwed up.
	 *
	 * scan directory and add associated users
	 */
	unlink(CRONUPDATE);
	if (chdir(CDir) < 0) {
		crondlog(DIE9 "chdir(%s)", CDir);
	}
	{
		DIR *dir = opendir(".");
		struct dirent *den;

		if (!dir)
			crondlog(DIE9 "chdir(%s)", "."); /* exits */
		while ((den = readdir(dir)) != NULL) {
			if (strchr(den->d_name, '.') != NULL) {
				continue;
			}
			if (getpwnam(den->d_name)) {
				SynchronizeFile(den->d_name);
			} else {
				crondlog(LVL7 "ignoring %s", den->d_name);
			}
		}
		closedir(dir);
	}
}

/*
 *  DeleteFile() - delete user database
 *
 *  Note: multiple entries for same user may exist if we were unable to
 *  completely delete a database due to running processes.
 */
static void DeleteFile(const char *userName)
{
	CronFile **pfile = &FileBase;
	CronFile *file;

	while ((file = *pfile) != NULL) {
		if (strcmp(userName, file->cf_User) == 0) {
			CronLine **pline = &file->cf_LineBase;
			CronLine *line;

			file->cf_Running = 0;
			file->cf_Deleted = 1;

			while ((line = *pline) != NULL) {
				if (line->cl_Pid > 0) {
					file->cf_Running = 1;
					pline = &line->cl_Next;
				} else {
					*pline = line->cl_Next;
					free(line->cl_Shell);
					free(line);
				}
			}
			if (file->cf_Running == 0) {
				*pfile = file->cf_Next;
				free(file->cf_User);
				free(file);
			} else {
				pfile = &file->cf_Next;
			}
		} else {
			pfile = &file->cf_Next;
		}
	}
}

/*
 * TestJobs()
 *
 * determine which jobs need to be run.  Under normal conditions, the
 * period is about a minute (one scan).  Worst case it will be one
 * hour (60 scans).
 */
static int TestJobs(time_t t1, time_t t2)
{
	int nJobs = 0;
	time_t t;

	/* Find jobs > t1 and <= t2 */

	for (t = t1 - t1 % 60; t <= t2; t += 60) {
		struct tm *ptm;
		CronFile *file;
		CronLine *line;

		if (t <= t1)
			continue;

		ptm = localtime(&t);
		for (file = FileBase; file; file = file->cf_Next) {
			if (DebugOpt)
				crondlog(LVL5 "file %s:", file->cf_User);
			if (file->cf_Deleted)
				continue;
			for (line = file->cf_LineBase; line; line = line->cl_Next) {
				if (DebugOpt)
					crondlog(LVL5 " line %s", line->cl_Shell);
				if (line->cl_Mins[ptm->tm_min] && line->cl_Hrs[ptm->tm_hour]
				 && (line->cl_Days[ptm->tm_mday] || line->cl_Dow[ptm->tm_wday])
				 && line->cl_Mons[ptm->tm_mon]
				) {
					if (DebugOpt) {
						crondlog(LVL5 " job: %d %s",
							(int)line->cl_Pid, line->cl_Shell);
					}
					if (line->cl_Pid > 0) {
						crondlog(LVL8 "user %s: process already running: %s",
							file->cf_User, line->cl_Shell);
					} else if (line->cl_Pid == 0) {
						line->cl_Pid = -1;
						file->cf_Ready = 1;
						++nJobs;
					}
				}
			}
		}
	}
	return nJobs;
}

static void RunJobs(void)
{
	CronFile *file;
	CronLine *line;

	for (file = FileBase; file; file = file->cf_Next) {
		if (!file->cf_Ready)
			continue;

		file->cf_Ready = 0;
		for (line = file->cf_LineBase; line; line = line->cl_Next) {
			if (line->cl_Pid >= 0)
				continue;

			RunJob(file->cf_User, line);
			crondlog(LVL8 "USER %s pid %3d cmd %s",
				file->cf_User, (int)line->cl_Pid, line->cl_Shell);
			if (line->cl_Pid < 0) {
				file->cf_Ready = 1;
			} else if (line->cl_Pid > 0) {
				file->cf_Running = 1;
			}
		}
	}
}

/*
 * CheckJobs() - check for job completion
 *
 * Check for job completion, return number of jobs still running after
 * all done.
 */
static int CheckJobs(void)
{
	CronFile *file;
	CronLine *line;
	int nStillRunning = 0;

	for (file = FileBase; file; file = file->cf_Next) {
		if (file->cf_Running) {
			file->cf_Running = 0;

			for (line = file->cf_LineBase; line; line = line->cl_Next) {
				int status, r;
				if (line->cl_Pid <= 0)
					continue;

				r = waitpid(line->cl_Pid, &status, WNOHANG);
				if (r < 0 || r == line->cl_Pid) {
					EndJob(file->cf_User, line);
					if (line->cl_Pid) {
						file->cf_Running = 1;
					}
				} else if (r == 0) {
					file->cf_Running = 1;
				}
			}
		}
		nStillRunning += file->cf_Running;
	}
	return nStillRunning;
}

#if ENABLE_FEATURE_CROND_CALL_SENDMAIL

// TODO: sendmail should be _run-time_ option, not compile-time!

static void
ForkJob(const char *user, CronLine *line, int mailFd,
		const char *prog, const char *cmd, const char *arg,
		const char *mail_filename)
{
	struct passwd *pas;
	pid_t pid;

	/* prepare things before vfork */
	pas = getpwnam(user);
	if (!pas) {
		crondlog(WARN9 "can't get uid for %s", user);
		goto err;
	}
	SetEnv(pas);

	pid = vfork();
	if (pid == 0) {
		/* CHILD */
		/* change running state to the user in question */
		ChangeUser(pas);
		if (DebugOpt) {
			crondlog(LVL5 "child running %s", prog);
		}
		if (mailFd >= 0) {
			xmove_fd(mailFd, mail_filename ? 1 : 0);
			dup2(1, 2);
		}
		/* crond 3.0pl1-100 puts tasks in separate process groups */
		bb_setpgrp();
		execlp(prog, prog, cmd, arg, (char *) NULL);
		crondlog(ERR20 "can't exec, user %s cmd %s %s %s", user, prog, cmd, arg);
		if (mail_filename) {
			fdprintf(1, "Exec failed: %s -c %s\n", prog, arg);
		}
		_exit(EXIT_SUCCESS);
	}

	line->cl_Pid = pid;
	if (pid < 0) {
		/* FORK FAILED */
		crondlog(ERR20 "can't vfork");
 err:
		line->cl_Pid = 0;
		if (mail_filename) {
			unlink(mail_filename);
		}
	} else if (mail_filename) {
		/* PARENT, FORK SUCCESS
		 * rename mail-file based on pid of process
		 */
		char mailFile2[128];

		snprintf(mailFile2, sizeof(mailFile2), "%s/cron.%s.%d", TMPDIR, user, pid);
		rename(mail_filename, mailFile2); // TODO: xrename?
	}

	/*
	 * Close the mail file descriptor.. we can't just leave it open in
	 * a structure, closing it later, because we might run out of descriptors
	 */
	if (mailFd >= 0) {
		close(mailFd);
	}
}

static void RunJob(const char *user, CronLine *line)
{
	char mailFile[128];
	int mailFd = -1;

	line->cl_Pid = 0;
	line->cl_MailFlag = 0;

	if (line->cl_MailTo) {
		/* open mail file - owner root so nobody can screw with it. */
		snprintf(mailFile, sizeof(mailFile), "%s/cron.%s.%d", TMPDIR, user, getpid());
		mailFd = open(mailFile, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL | O_APPEND, 0600);

		if (mailFd >= 0) {
			line->cl_MailFlag = 1;
			fdprintf(mailFd, "To: %s\nSubject: cron: %s\n\n", line->cl_MailTo,
				line->cl_Shell);
			line->cl_MailPos = lseek(mailFd, 0, SEEK_CUR);
		} else {
			crondlog(ERR20 "can't create mail file %s for user %s, "
					"discarding output", mailFile, user);
		}
	}

	ForkJob(user, line, mailFd, DEFAULT_SHELL, "-c", line->cl_Shell, mailFile);
}

/*
 * EndJob - called when job terminates and when mail terminates
 */
static void EndJob(const char *user, CronLine *line)
{
	int mailFd;
	char mailFile[128];
	struct stat sbuf;

	/* No job */
	if (line->cl_Pid <= 0) {
		line->cl_Pid = 0;
		return;
	}

	/*
	 * End of job and no mail file
	 * End of sendmail job
	 */
	snprintf(mailFile, sizeof(mailFile), "%s/cron.%s.%d", TMPDIR, user, line->cl_Pid);
	line->cl_Pid = 0;

	if (line->cl_MailFlag == 0) {
		return;
	}
	line->cl_MailFlag = 0;

	/*
	 * End of primary job - check for mail file.  If size has increased and
	 * the file is still valid, we sendmail it.
	 */
	mailFd = open(mailFile, O_RDONLY);
	unlink(mailFile);
	if (mailFd < 0) {
		return;
	}

	if (fstat(mailFd, &sbuf) < 0 || sbuf.st_uid != DaemonUid
	 || sbuf.st_nlink != 0 || sbuf.st_size == line->cl_MailPos
	 || !S_ISREG(sbuf.st_mode)
	) {
		close(mailFd);
		return;
	}
	if (line->cl_MailTo)
		ForkJob(user, line, mailFd, SENDMAIL, SENDMAIL_ARGS, NULL);
}

#else /* crond without sendmail */

static void RunJob(const char *user, CronLine *line)
{
	struct passwd *pas;
	pid_t pid;

	/* prepare things before vfork */
	pas = getpwnam(user);
	if (!pas) {
		crondlog(WARN9 "can't get uid for %s", user);
		goto err;
	}
	SetEnv(pas);

	/* fork as the user in question and run program */
	pid = vfork();
	if (pid == 0) {
		/* CHILD */
		/* change running state to the user in question */
		ChangeUser(pas);
		if (DebugOpt) {
			crondlog(LVL5 "child running %s", DEFAULT_SHELL);
		}
		/* crond 3.0pl1-100 puts tasks in separate process groups */
		bb_setpgrp();
		execl(DEFAULT_SHELL, DEFAULT_SHELL, "-c", line->cl_Shell, (char *) NULL);
		crondlog(ERR20 "can't exec, user %s cmd %s %s %s", user,
				 DEFAULT_SHELL, "-c", line->cl_Shell);
		_exit(EXIT_SUCCESS);
	}
	if (pid < 0) {
		/* FORK FAILED */
		crondlog(ERR20 "can't vfork");
 err:
		pid = 0;
	}
	line->cl_Pid = pid;
}

#endif /* ENABLE_FEATURE_CROND_CALL_SENDMAIL */
