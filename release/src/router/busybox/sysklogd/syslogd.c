/* vi: set sw=4 ts=4: */
/*
 * Mini syslogd implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define syslogd_trivial_usage
//usage:       "[OPTIONS]"
//usage:#define syslogd_full_usage "\n\n"
//usage:       "System logging utility\n"
//usage:	IF_NOT_FEATURE_SYSLOGD_CFG(
//usage:       "(this version of syslogd ignores /etc/syslog.conf)\n"
//usage:	)
//usage:     "\n	-n		Run in foreground"
//usage:     "\n	-O FILE		Log to FILE (default:/var/log/messages)"
//usage:     "\n	-l N		Log only messages more urgent than prio N (1-8)"
//usage:     "\n	-S		Smaller output"
//usage:	IF_FEATURE_ROTATE_LOGFILE(
//usage:     "\n	-s SIZE		Max size (KB) before rotation (default:200KB, 0=off)"
//usage:     "\n	-b N		N rotated logs to keep (default:1, max=99, 0=purge)"
//usage:	)
//usage:	IF_FEATURE_REMOTE_LOG(
//usage:     "\n	-R HOST[:PORT]	Log to IP or hostname on PORT (default PORT=514/UDP)"
//usage:     "\n	-L		Log locally and via network (default is network only if -R)"
//usage:	)
//usage:	IF_FEATURE_SYSLOGD_DUP(
//usage:     "\n	-D		Drop duplicates"
//usage:	)
//usage:	IF_FEATURE_IPC_SYSLOG(
/* NB: -Csize shouldn't have space (because size is optional) */
//usage:     "\n	-C[size_kb]	Log to shared mem buffer (use logread to read it)"
//usage:	)
//usage:	IF_FEATURE_SYSLOGD_CFG(
//usage:     "\n	-f FILE		Use FILE as config (default:/etc/syslog.conf)"
//usage:	)
/* //usage:  "\n	-m MIN		Minutes between MARK lines (default:20, 0=off)" */
//usage:
//usage:#define syslogd_example_usage
//usage:       "$ syslogd -R masterlog:514\n"
//usage:       "$ syslogd -R 192.168.1.1:601\n"

/*
 * Done in syslogd_and_logger.c:
#include "libbb.h"
#define SYSLOG_NAMES
#define SYSLOG_NAMES_CONST
#include <syslog.h>
*/

#include <sys/un.h>
#include <sys/uio.h>

#if ENABLE_FEATURE_REMOTE_LOG
#include <netinet/in.h>
#endif

#if ENABLE_FEATURE_IPC_SYSLOG
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#endif


#define DEBUG 0

/* MARK code is not very useful, is bloat, and broken:
 * can deadlock if alarmed to make MARK while writing to IPC buffer
 * (semaphores are down but do_mark routine tries to down them again) */
#undef SYSLOGD_MARK

/* Write locking does not seem to be useful either */
#undef SYSLOGD_WRLOCK

enum {
	MAX_READ = CONFIG_FEATURE_SYSLOGD_READ_BUFFER_SIZE,
	DNS_WAIT_SEC = 2 * 60,
};

/* Semaphore operation structures */
struct shbuf_ds {
	int32_t size;   /* size of data - 1 */
	int32_t tail;   /* end of message list */
	char data[1];   /* data/messages */
};

#if ENABLE_FEATURE_REMOTE_LOG
typedef struct {
	int remoteFD;
	unsigned last_dns_resolve;
	len_and_sockaddr *remoteAddr;
	const char *remoteHostname;
} remoteHost_t;
#endif

typedef struct logFile_t {
	const char *path;
	int fd;
#if ENABLE_FEATURE_ROTATE_LOGFILE
	unsigned size;
	uint8_t isRegular;
#endif
} logFile_t;

#if ENABLE_FEATURE_SYSLOGD_CFG
typedef struct logRule_t {
	uint8_t enabled_facility_priomap[LOG_NFACILITIES];
	struct logFile_t *file;
	struct logRule_t *next;
} logRule_t;
#endif

/* Allows us to have smaller initializer. Ugly. */
#define GLOBALS \
	logFile_t logFile;                      \
	/* interval between marks in seconds */ \
	/*int markInterval;*/                   \
	/* level of messages to be logged */    \
	int logLevel;                           \
IF_FEATURE_ROTATE_LOGFILE( \
	/* max size of file before rotation */  \
	unsigned logFileSize;                   \
	/* number of rotated message files */   \
	unsigned logFileRotate;                 \
) \
IF_FEATURE_IPC_SYSLOG( \
	int shmid; /* ipc shared memory id */   \
	int s_semid; /* ipc semaphore id */     \
	int shm_size;                           \
	struct sembuf SMwup[1];                 \
	struct sembuf SMwdn[3];                 \
) \
IF_FEATURE_SYSLOGD_CFG( \
	logRule_t *log_rules; \
)

struct init_globals {
	GLOBALS
};

struct globals {
	GLOBALS

#if ENABLE_FEATURE_REMOTE_LOG
	llist_t *remoteHosts;
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	struct shbuf_ds *shbuf;
#endif
	time_t last_log_time;
	/* localhost's name. We print only first 64 chars */
	char *hostname;

	/* We recv into recvbuf... */
	char recvbuf[MAX_READ * (1 + ENABLE_FEATURE_SYSLOGD_DUP)];
	/* ...then copy to parsebuf, escaping control chars */
	/* (can grow x2 max) */
	char parsebuf[MAX_READ*2];
	/* ...then sprintf into printbuf, adding timestamp (15 chars),
	 * host (64), fac.prio (20) to the message */
	/* (growth by: 15 + 64 + 20 + delims = ~110) */
	char printbuf[MAX_READ*2 + 128];
};

static const struct init_globals init_data = {
	.logFile = {
		.path = "/var/log/messages",
		.fd = -1,
	},
#ifdef SYSLOGD_MARK
	.markInterval = 20 * 60,
#endif
	.logLevel = 8,
#if ENABLE_FEATURE_ROTATE_LOGFILE
	.logFileSize = 200 * 1024,
	.logFileRotate = 1,
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	.shmid = -1,
	.s_semid = -1,
	.shm_size = ((CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE)*1024), /* default shm size */
	.SMwup = { {1, -1, IPC_NOWAIT} },
	.SMwdn = { {0, 0}, {1, 0}, {1, +1} },
#endif
};

#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(memcpy(xzalloc(sizeof(G)), &init_data, sizeof(init_data))); \
} while (0)


/* Options */
enum {
	OPTBIT_mark = 0, // -m
	OPTBIT_nofork, // -n
	OPTBIT_outfile, // -O
	OPTBIT_loglevel, // -l
	OPTBIT_small, // -S
	IF_FEATURE_ROTATE_LOGFILE(OPTBIT_filesize   ,)	// -s
	IF_FEATURE_ROTATE_LOGFILE(OPTBIT_rotatecnt  ,)	// -b
	IF_FEATURE_REMOTE_LOG(    OPTBIT_remotelog  ,)	// -R
	IF_FEATURE_REMOTE_LOG(    OPTBIT_locallog   ,)	// -L
	IF_FEATURE_IPC_SYSLOG(    OPTBIT_circularlog,)	// -C
	IF_FEATURE_SYSLOGD_DUP(   OPTBIT_dup        ,)	// -D
	IF_FEATURE_SYSLOGD_CFG(   OPTBIT_cfg        ,)	// -f

	OPT_mark        = 1 << OPTBIT_mark    ,
	OPT_nofork      = 1 << OPTBIT_nofork  ,
	OPT_outfile     = 1 << OPTBIT_outfile ,
	OPT_loglevel    = 1 << OPTBIT_loglevel,
	OPT_small       = 1 << OPTBIT_small   ,
	OPT_filesize    = IF_FEATURE_ROTATE_LOGFILE((1 << OPTBIT_filesize   )) + 0,
	OPT_rotatecnt   = IF_FEATURE_ROTATE_LOGFILE((1 << OPTBIT_rotatecnt  )) + 0,
	OPT_remotelog   = IF_FEATURE_REMOTE_LOG(    (1 << OPTBIT_remotelog  )) + 0,
	OPT_locallog    = IF_FEATURE_REMOTE_LOG(    (1 << OPTBIT_locallog   )) + 0,
	OPT_circularlog = IF_FEATURE_IPC_SYSLOG(    (1 << OPTBIT_circularlog)) + 0,
	OPT_dup         = IF_FEATURE_SYSLOGD_DUP(   (1 << OPTBIT_dup        )) + 0,
	OPT_cfg         = IF_FEATURE_SYSLOGD_CFG(   (1 << OPTBIT_cfg        )) + 0,
};
#define OPTION_STR "m:nO:l:S" \
	IF_FEATURE_ROTATE_LOGFILE("s:" ) \
	IF_FEATURE_ROTATE_LOGFILE("b:" ) \
	IF_FEATURE_REMOTE_LOG(    "R:" ) \
	IF_FEATURE_REMOTE_LOG(    "L"  ) \
	IF_FEATURE_IPC_SYSLOG(    "C::") \
	IF_FEATURE_SYSLOGD_DUP(   "D"  ) \
	IF_FEATURE_SYSLOGD_CFG(   "f:"  )
#define OPTION_DECL *opt_m, *opt_l \
	IF_FEATURE_ROTATE_LOGFILE(,*opt_s) \
	IF_FEATURE_ROTATE_LOGFILE(,*opt_b) \
	IF_FEATURE_IPC_SYSLOG(    ,*opt_C = NULL) \
	IF_FEATURE_SYSLOGD_CFG(   ,*opt_f = NULL)
#define OPTION_PARAM &opt_m, &(G.logFile.path), &opt_l \
	IF_FEATURE_ROTATE_LOGFILE(,&opt_s) \
	IF_FEATURE_ROTATE_LOGFILE(,&opt_b) \
	IF_FEATURE_REMOTE_LOG(	  ,&remoteAddrList) \
	IF_FEATURE_IPC_SYSLOG(    ,&opt_C) \
	IF_FEATURE_SYSLOGD_CFG(   ,&opt_f)


#if ENABLE_FEATURE_SYSLOGD_CFG
static const CODE* find_by_name(char *name, const CODE* c_set)
{
	for (; c_set->c_name; c_set++) {
		if (strcmp(name, c_set->c_name) == 0)
			return c_set;
	}
	return NULL;
}
#endif
static const CODE* find_by_val(int val, const CODE* c_set)
{
	for (; c_set->c_name; c_set++) {
		if (c_set->c_val == val)
			return c_set;
	}
	return NULL;
}

#if ENABLE_FEATURE_SYSLOGD_CFG
static void parse_syslogdcfg(const char *file)
{
	char *t;
	logRule_t **pp_rule;
	/* tok[0] set of selectors */
	/* tok[1] file name */
	/* tok[2] has to be NULL */
	char *tok[3];
	parser_t *parser;

	parser = config_open2(file ? file : "/etc/syslog.conf",
				file ? xfopen_for_read : fopen_for_read);
	if (!parser)
		/* didn't find default /etc/syslog.conf */
		/* proceed as if we built busybox without config support */
		return;

	/* use ptr to ptr to avoid checking whether head was initialized */
	pp_rule = &G.log_rules;
	/* iterate through lines of config, skipping comments */
	while (config_read(parser, tok, 3, 2, "# \t", PARSE_NORMAL | PARSE_MIN_DIE)) {
		char *cur_selector;
		logRule_t *cur_rule;

		/* unexpected trailing token? */
		if (tok[2])
			goto cfgerr;

		cur_rule = *pp_rule = xzalloc(sizeof(*cur_rule));

		cur_selector = tok[0];
		/* iterate through selectors: "kern.info;kern.!err;..." */
		do {
			const CODE *code;
			char *next_selector;
			uint8_t negated_prio; /* "kern.!err" */
			uint8_t single_prio;  /* "kern.=err" */
			uint32_t facmap; /* bitmap of enabled facilities */
			uint8_t primap;  /* bitmap of enabled priorities */
			unsigned i;

			next_selector = strchr(cur_selector, ';');
			if (next_selector)
				*next_selector++ = '\0';

			t = strchr(cur_selector, '.');
			if (!t)
				goto cfgerr;
			*t++ = '\0'; /* separate facility from priority */

			negated_prio = 0;
			single_prio = 0;
			if (*t == '!') {
				negated_prio = 1;
				++t;
			}
			if (*t == '=') {
				single_prio = 1;
				++t;
			}

			/* parse priority */
			if (*t == '*')
				primap = 0xff; /* all 8 log levels enabled */
			else {
				uint8_t priority;
				code = find_by_name(t, prioritynames);
				if (!code)
					goto cfgerr;
				primap = 0;
				priority = code->c_val;
				if (priority == INTERNAL_NOPRI) {
					/* ensure we take "enabled_facility_priomap[fac] &= 0" branch below */
					negated_prio = 1;
				} else {
					priority = 1 << priority;
					do {
						primap |= priority;
						if (single_prio)
							break;
						priority >>= 1;
					} while (priority);
					if (negated_prio)
						primap = ~primap;
				}
			}

			/* parse facility */
			if (*cur_selector == '*')
				facmap = (1<<LOG_NFACILITIES) - 1;
			else {
				char *next_facility;
				facmap = 0;
				t = cur_selector;
				/* iterate through facilities: "kern,daemon.<priospec>" */
				do {
					next_facility = strchr(t, ',');
					if (next_facility)
						*next_facility++ = '\0';
					code = find_by_name(t, facilitynames);
					if (!code)
						goto cfgerr;
					/* "mark" is not a real facility, skip it */
					if (code->c_val != INTERNAL_MARK)
						facmap |= 1<<(LOG_FAC(code->c_val));
					t = next_facility;
				} while (t);
			}

			/* merge result with previous selectors */
			for (i = 0; i < LOG_NFACILITIES; ++i) {
				if (!(facmap & (1<<i)))
					continue;
				if (negated_prio)
					cur_rule->enabled_facility_priomap[i] &= primap;
				else
					cur_rule->enabled_facility_priomap[i] |= primap;
			}

			cur_selector = next_selector;
		} while (cur_selector);

		/* check whether current file name was mentioned in previous rules or
		 * as global logfile (G.logFile).
		 */
		if (strcmp(G.logFile.path, tok[1]) == 0) {
			cur_rule->file = &G.logFile;
			goto found;
		}
		/* temporarily use cur_rule as iterator, but *pp_rule still points
		 * to currently processing rule entry.
		 * NOTE: *pp_rule points to the current (and last in the list) rule.
		 */
		for (cur_rule = G.log_rules; cur_rule != *pp_rule; cur_rule = cur_rule->next) {
			if (strcmp(cur_rule->file->path, tok[1]) == 0) {
				/* found - reuse the same file structure */
				(*pp_rule)->file = cur_rule->file;
				cur_rule = *pp_rule;
				goto found;
			}
		}
		cur_rule->file = xzalloc(sizeof(*cur_rule->file));
		cur_rule->file->fd = -1;
		cur_rule->file->path = xstrdup(tok[1]);
 found:
		pp_rule = &cur_rule->next;
	}
	config_close(parser);
	return;

 cfgerr:
	bb_error_msg_and_die("error in '%s' at line %d", file, parser->lineno);
}
#endif

/* circular buffer variables/structures */
#if ENABLE_FEATURE_IPC_SYSLOG

#if CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE < 4
#error Sorry, you must set the syslogd buffer size to at least 4KB.
#error Please check CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE
#endif

/* our shared key (syslogd.c and logread.c must be in sync) */
enum { KEY_ID = 0x414e4547 }; /* "GENA" */

static void ipcsyslog_cleanup(void)
{
	if (G.shmid != -1) {
		shmdt(G.shbuf);
	}
	if (G.shmid != -1) {
		shmctl(G.shmid, IPC_RMID, NULL);
	}
	if (G.s_semid != -1) {
		semctl(G.s_semid, 0, IPC_RMID, 0);
	}
}

static void ipcsyslog_init(void)
{
	if (DEBUG)
		printf("shmget(%x, %d,...)\n", (int)KEY_ID, G.shm_size);

	G.shmid = shmget(KEY_ID, G.shm_size, IPC_CREAT | 0644);
	if (G.shmid == -1) {
		bb_perror_msg_and_die("shmget");
	}

	G.shbuf = shmat(G.shmid, NULL, 0);
	if (G.shbuf == (void*) -1L) { /* shmat has bizarre error return */
		bb_perror_msg_and_die("shmat");
	}

	memset(G.shbuf, 0, G.shm_size);
	G.shbuf->size = G.shm_size - offsetof(struct shbuf_ds, data) - 1;
	/*G.shbuf->tail = 0;*/

	/* we'll trust the OS to set initial semval to 0 (let's hope) */
	G.s_semid = semget(KEY_ID, 2, IPC_CREAT | IPC_EXCL | 1023);
	if (G.s_semid == -1) {
		if (errno == EEXIST) {
			G.s_semid = semget(KEY_ID, 2, 0);
			if (G.s_semid != -1)
				return;
		}
		bb_perror_msg_and_die("semget");
	}
}

/* Write message to shared mem buffer */
static void log_to_shmem(const char *msg)
{
	int old_tail, new_tail;
	int len;

	if (semop(G.s_semid, G.SMwdn, 3) == -1) {
		bb_perror_msg_and_die("SMwdn");
	}

	/* Circular Buffer Algorithm:
	 * --------------------------
	 * tail == position where to store next syslog message.
	 * tail's max value is (shbuf->size - 1)
	 * Last byte of buffer is never used and remains NUL.
	 */
	len = strlen(msg) + 1; /* length with NUL included */
 again:
	old_tail = G.shbuf->tail;
	new_tail = old_tail + len;
	if (new_tail < G.shbuf->size) {
		/* store message, set new tail */
		memcpy(G.shbuf->data + old_tail, msg, len);
		G.shbuf->tail = new_tail;
	} else {
		/* k == available buffer space ahead of old tail */
		int k = G.shbuf->size - old_tail;
		/* copy what fits to the end of buffer, and repeat */
		memcpy(G.shbuf->data + old_tail, msg, k);
		msg += k;
		len -= k;
		G.shbuf->tail = 0;
		goto again;
	}
	if (semop(G.s_semid, G.SMwup, 1) == -1) {
		bb_perror_msg_and_die("SMwup");
	}
	if (DEBUG)
		printf("tail:%d\n", G.shbuf->tail);
}
#else
void ipcsyslog_cleanup(void);
void ipcsyslog_init(void);
void log_to_shmem(const char *msg);
#endif /* FEATURE_IPC_SYSLOG */

/* Print a message to the log file. */
static void log_locally(time_t now, char *msg, logFile_t *log_file)
{
#ifdef SYSLOGD_WRLOCK
	struct flock fl;
#endif
	int len = strlen(msg);

	if (log_file->fd >= 0) {
		/* Reopen log file every second. This allows admin
		 * to delete the file and not worry about restarting us.
		 * This costs almost nothing since it happens
		 * _at most_ once a second.
		 */
		if (!now)
			now = time(NULL);
		if (G.last_log_time != now) {
			G.last_log_time = now;
			close(log_file->fd);
			goto reopen;
		}
	} else {
 reopen:
		log_file->fd = open(log_file->path, O_WRONLY | O_CREAT
					| O_NOCTTY | O_APPEND | O_NONBLOCK,
					0666);
		if (log_file->fd < 0) {
			/* cannot open logfile? - print to /dev/console then */
			int fd = device_open(DEV_CONSOLE, O_WRONLY | O_NOCTTY | O_NONBLOCK);
			if (fd < 0)
				fd = 2; /* then stderr, dammit */
			full_write(fd, msg, len);
			if (fd != 2)
				close(fd);
			return;
		}
#if ENABLE_FEATURE_ROTATE_LOGFILE
		{
			struct stat statf;
			log_file->isRegular = (fstat(log_file->fd, &statf) == 0 && S_ISREG(statf.st_mode));
			/* bug (mostly harmless): can wrap around if file > 4gb */
			log_file->size = statf.st_size;
		}
#endif
	}

#ifdef SYSLOGD_WRLOCK
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	fl.l_type = F_WRLCK;
	fcntl(log_file->fd, F_SETLKW, &fl);
#endif

#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (G.logFileSize && log_file->isRegular && log_file->size > G.logFileSize) {
		if (G.logFileRotate) { /* always 0..1 */
			int i = strlen(log_file->path) + 3 + 1;
			char oldFile[i];

			sprintf(oldFile, "%s-1", log_file->path);
			unlink(oldFile);
			rename(log_file->path, oldFile);

#ifdef SYSLOGD_WRLOCK
			fl.l_type = F_UNLCK;
			fcntl(log_file->fd, F_SETLKW, &fl);
#endif
			close(log_file->fd);
			goto reopen;
		}
		ftruncate(log_file->fd, 0);
	}
	log_file->size +=
#endif
			full_write(log_file->fd, msg, len);
#ifdef SYSLOGD_WRLOCK
	fl.l_type = F_UNLCK;
	fcntl(log_file->fd, F_SETLKW, &fl);
#endif
}

static void parse_fac_prio_20(int pri, char *res20)
{
	const CODE *c_pri, *c_fac;

	c_fac = find_by_val(LOG_FAC(pri) << 3, facilitynames);
	if (c_fac) {
		c_pri = find_by_val(LOG_PRI(pri), prioritynames);
		if (c_pri) {
			snprintf(res20, 20, "%s.%s", c_fac->c_name, c_pri->c_name);
			return;
		}
	}
	snprintf(res20, 20, "<%d>", pri);
}

/* len parameter is used only for "is there a timestamp?" check.
 * NB: some callers cheat and supply len==0 when they know
 * that there is no timestamp, short-circuiting the test. */
static void timestamp_and_log(int pri, char *msg, int len)
{
	char *timestamp;
	time_t now;

	/* Jan 18 00:11:22 msg... */
	/* 01234567890123456 */
	if (len < 16 || msg[3] != ' ' || msg[6] != ' '
	 || msg[9] != ':' || msg[12] != ':' || msg[15] != ' '
	) {
		time(&now);
		timestamp = ctime(&now) + 4; /* skip day of week */
	} else {
		now = 0;
		timestamp = msg;
		msg += 16;
	}
	timestamp[15] = '\0';

	if (option_mask32 & OPT_small)
		sprintf(G.printbuf, "%s %s\n", timestamp, msg);
	else {
		char res[20];
		parse_fac_prio_20(pri, res);
		sprintf(G.printbuf, "%s %.64s %s %s\n", timestamp, G.hostname, res, msg);
	}

	/* Log message locally (to file or shared mem) */
#if ENABLE_FEATURE_SYSLOGD_CFG
	{
		bool match = 0;
		logRule_t *rule;
		uint8_t facility = LOG_FAC(pri);
		uint8_t prio_bit = 1 << LOG_PRI(pri);

		for (rule = G.log_rules; rule; rule = rule->next) {
			if (rule->enabled_facility_priomap[facility] & prio_bit) {
				log_locally(now, G.printbuf, rule->file);
				match = 1;
			}
		}
		if (match)
			return;
	}
#endif
	if (LOG_PRI(pri) < G.logLevel) {
#if ENABLE_FEATURE_IPC_SYSLOG
		if ((option_mask32 & OPT_circularlog) && G.shbuf) {
			log_to_shmem(G.printbuf);
			return;
		}
#endif
		log_locally(now, G.printbuf, &G.logFile);
	}
}

static void timestamp_and_log_internal(const char *msg)
{
	/* -L, or no -R */
	if (ENABLE_FEATURE_REMOTE_LOG && !(option_mask32 & OPT_locallog))
		return;
	timestamp_and_log(LOG_SYSLOG | LOG_INFO, (char*)msg, 0);
}

/* tmpbuf[len] is a NUL byte (set by caller), but there can be other,
 * embedded NULs. Split messages on each of these NULs, parse prio,
 * escape control chars and log each locally. */
static void split_escape_and_log(char *tmpbuf, int len)
{
	char *p = tmpbuf;

	tmpbuf += len;
	while (p < tmpbuf) {
		char c;
		char *q = G.parsebuf;
		int pri = (LOG_USER | LOG_NOTICE);

		if (*p == '<') {
			/* Parse the magic priority number */
			pri = bb_strtou(p + 1, &p, 10);
			if (*p == '>')
				p++;
			if (pri & ~(LOG_FACMASK | LOG_PRIMASK))
				pri = (LOG_USER | LOG_NOTICE);
		}

		while ((c = *p++)) {
			if (c == '\n')
				c = ' ';
			if (!(c & ~0x1f) && c != '\t') {
				*q++ = '^';
				c += '@'; /* ^@, ^A, ^B... */
			}
			*q++ = c;
		}
		*q = '\0';

		/* Now log it */
		timestamp_and_log(pri, G.parsebuf, q - G.parsebuf);
	}
}

#ifdef SYSLOGD_MARK
static void do_mark(int sig)
{
	if (G.markInterval) {
		timestamp_and_log_internal("-- MARK --");
		alarm(G.markInterval);
	}
}
#endif

/* Don't inline: prevent struct sockaddr_un to take up space on stack
 * permanently */
static NOINLINE int create_socket(void)
{
	struct sockaddr_un sunx;
	int sock_fd;
	char *dev_log_name;

#if ENABLE_FEATURE_SYSTEMD
	if (sd_listen_fds() == 1)
		return SD_LISTEN_FDS_START;
#endif

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;

	/* Unlink old /dev/log or object it points to. */
	/* (if it exists, bind will fail) */
	strcpy(sunx.sun_path, "/dev/log");
	dev_log_name = xmalloc_follow_symlinks("/dev/log");
	if (dev_log_name) {
		safe_strncpy(sunx.sun_path, dev_log_name, sizeof(sunx.sun_path));
		free(dev_log_name);
	}
	unlink(sunx.sun_path);

	sock_fd = xsocket(AF_UNIX, SOCK_DGRAM, 0);
	xbind(sock_fd, (struct sockaddr *) &sunx, sizeof(sunx));
	chmod("/dev/log", 0666);

	return sock_fd;
}

#if ENABLE_FEATURE_REMOTE_LOG
static int try_to_resolve_remote(remoteHost_t *rh)
{
	if (!rh->remoteAddr) {
		unsigned now = monotonic_sec();

		/* Don't resolve name too often - DNS timeouts can be big */
		if ((now - rh->last_dns_resolve) < DNS_WAIT_SEC)
			return -1;
		rh->last_dns_resolve = now;
		rh->remoteAddr = host2sockaddr(rh->remoteHostname, 514);
		if (!rh->remoteAddr)
			return -1;
	}
	return xsocket(rh->remoteAddr->u.sa.sa_family, SOCK_DGRAM, 0);
}
#endif

static void do_syslogd(void) NORETURN;
static void do_syslogd(void)
{
	int sock_fd;
#if ENABLE_FEATURE_REMOTE_LOG
	llist_t *item;
#endif
#if ENABLE_FEATURE_SYSLOGD_DUP
	int last_sz = -1;
	char *last_buf;
	char *recvbuf = G.recvbuf;
#else
#define recvbuf (G.recvbuf)
#endif

	/* Set up signal handlers (so that they interrupt read()) */
	signal_no_SA_RESTART_empty_mask(SIGTERM, record_signo);
	signal_no_SA_RESTART_empty_mask(SIGINT, record_signo);
	//signal_no_SA_RESTART_empty_mask(SIGQUIT, record_signo);
	signal(SIGHUP, SIG_IGN);
#ifdef SYSLOGD_MARK
	signal(SIGALRM, do_mark);
	alarm(G.markInterval);
#endif
	sock_fd = create_socket();

	if (ENABLE_FEATURE_IPC_SYSLOG && (option_mask32 & OPT_circularlog)) {
		ipcsyslog_init();
	}

	timestamp_and_log_internal("syslogd started: BusyBox v" BB_VER);

	while (!bb_got_signal) {
		ssize_t sz;

#if ENABLE_FEATURE_SYSLOGD_DUP
		last_buf = recvbuf;
		if (recvbuf == G.recvbuf)
			recvbuf = G.recvbuf + MAX_READ;
		else
			recvbuf = G.recvbuf;
#endif
 read_again:
		sz = read(sock_fd, recvbuf, MAX_READ - 1);
		if (sz < 0) {
			if (!bb_got_signal)
				bb_perror_msg("read from /dev/log");
			break;
		}

		/* Drop trailing '\n' and NULs (typically there is one NUL) */
		while (1) {
			if (sz == 0)
				goto read_again;
			/* man 3 syslog says: "A trailing newline is added when needed".
			 * However, neither glibc nor uclibc do this:
			 * syslog(prio, "test")   sends "test\0" to /dev/log,
			 * syslog(prio, "test\n") sends "test\n\0".
			 * IOW: newline is passed verbatim!
			 * I take it to mean that it's syslogd's job
			 * to make those look identical in the log files. */
			if (recvbuf[sz-1] != '\0' && recvbuf[sz-1] != '\n')
				break;
			sz--;
		}
#if ENABLE_FEATURE_SYSLOGD_DUP
		if ((option_mask32 & OPT_dup) && (sz == last_sz))
			if (memcmp(last_buf, recvbuf, sz) == 0)
				continue;
		last_sz = sz;
#endif
#if ENABLE_FEATURE_REMOTE_LOG
		/* Stock syslogd sends it '\n'-terminated
		 * over network, mimic that */
		recvbuf[sz] = '\n';

		/* We are not modifying log messages in any way before send */
		/* Remote site cannot trust _us_ anyway and need to do validation again */
		for (item = G.remoteHosts; item != NULL; item = item->link) {
			remoteHost_t *rh = (remoteHost_t *)item->data;

			if (rh->remoteFD == -1) {
				rh->remoteFD = try_to_resolve_remote(rh);
				if (rh->remoteFD == -1)
					continue;
			}

			/* Send message to remote logger.
			 * On some errors, close and set remoteFD to -1
			 * so that DNS resolution is retried.
			 */
			if (sendto(rh->remoteFD, recvbuf, sz+1,
					MSG_DONTWAIT | MSG_NOSIGNAL,
					&(rh->remoteAddr->u.sa), rh->remoteAddr->len) == -1
			) {
				switch (errno) {
				case ECONNRESET:
				case ENOTCONN: /* paranoia */
				case EPIPE:
					close(rh->remoteFD);
					rh->remoteFD = -1;
					free(rh->remoteAddr);
					rh->remoteAddr = NULL;
				}
			}
		}
#endif
		if (!ENABLE_FEATURE_REMOTE_LOG || (option_mask32 & OPT_locallog)) {
			recvbuf[sz] = '\0'; /* ensure it *is* NUL terminated */
			split_escape_and_log(recvbuf, sz);
		}
	} /* while (!bb_got_signal) */

	timestamp_and_log_internal("syslogd exiting");
	puts("syslogd exiting");
	if (ENABLE_FEATURE_IPC_SYSLOG)
		ipcsyslog_cleanup();
	kill_myself_with_sig(bb_got_signal);
#undef recvbuf
}

int syslogd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int syslogd_main(int argc UNUSED_PARAM, char **argv)
{
	int opts;
	char OPTION_DECL;
#if ENABLE_FEATURE_REMOTE_LOG
	llist_t *remoteAddrList = NULL;
#endif

	INIT_G();

	/* No non-option params, -R can occur multiple times */
	opt_complementary = "=0" IF_FEATURE_REMOTE_LOG(":R::");
	opts = getopt32(argv, OPTION_STR, OPTION_PARAM);
#if ENABLE_FEATURE_REMOTE_LOG
	while (remoteAddrList) {
		remoteHost_t *rh = xzalloc(sizeof(*rh));
		rh->remoteHostname = llist_pop(&remoteAddrList);
		rh->remoteFD = -1;
		rh->last_dns_resolve = monotonic_sec() - DNS_WAIT_SEC - 1;
		llist_add_to(&G.remoteHosts, rh);
	}
#endif

#ifdef SYSLOGD_MARK
	if (opts & OPT_mark) // -m
		G.markInterval = xatou_range(opt_m, 0, INT_MAX/60) * 60;
#endif
	//if (opts & OPT_nofork) // -n
	//if (opts & OPT_outfile) // -O
	if (opts & OPT_loglevel) // -l
		G.logLevel = xatou_range(opt_l, 1, 8);
	if (opts & OPT_small) // -S
		option_mask32 |= OPT_small;     // make syslog smaller.
#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (opts & OPT_filesize) // -s
		G.logFileSize = xatou_range(opt_s, 0, INT_MAX/1024) * 1024;
	if (opts & OPT_rotatecnt) // -b
		G.logFileRotate = xatou_range(opt_b, 0, 1);
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	if (opt_C) // -Cn
		G.shm_size = xatoul_range(opt_C, 4, INT_MAX/1024) * 1024;
#endif
	/* If they have not specified remote logging, then log locally */
	if (ENABLE_FEATURE_REMOTE_LOG && !(opts & OPT_remotelog)) // -R
		option_mask32 |= OPT_locallog;
#if ENABLE_FEATURE_SYSLOGD_CFG
	parse_syslogdcfg(opt_f);
#endif

	/* Store away localhost's name before the fork */
	G.hostname = safe_gethostname();
	*strchrnul(G.hostname, '.') = '\0';

	if (!(opts & OPT_nofork)) {
		bb_daemonize_or_rexec(DAEMON_CHDIR_ROOT, argv);
	}
	//umask(0); - why??
	write_pidfile("/var/run/syslogd.pid");
	do_syslogd();
	/* return EXIT_SUCCESS; */
}

/* Clean up. Needed because we are included from syslogd_and_logger.c */
#undef DEBUG
#undef SYSLOGD_MARK
#undef SYSLOGD_WRLOCK
#undef G
#undef GLOBALS
#undef INIT_G
#undef OPTION_STR
#undef OPTION_DECL
#undef OPTION_PARAM
