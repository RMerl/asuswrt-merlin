/* vi: set sw=4 ts=4: */
/*
 * tiny-ls.c version 0.1.0: A minimalist 'ls'
 * Copyright (C) 1996 Brian Candler <B.Candler@pobox.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* [date unknown. Perhaps before year 2000]
 * To achieve a small memory footprint, this version of 'ls' doesn't do any
 * file sorting, and only has the most essential command line switches
 * (i.e., the ones I couldn't live without :-) All features which involve
 * linking in substantial chunks of libc can be disabled.
 *
 * Although I don't really want to add new features to this program to
 * keep it small, I *am* interested to receive bug fixes and ways to make
 * it more portable.
 *
 * KNOWN BUGS:
 * 1. hidden files can make column width too large
 *
 * NON-OPTIMAL BEHAVIOUR:
 * 1. autowidth reads directories twice
 * 2. if you do a short directory listing without filetype characters
 *    appended, there's no need to stat each one
 * PORTABILITY:
 * 1. requires lstat (BSD) - how do you do it without?
 *
 * [2009-03]
 * ls sorts listing now, and supports almost all options.
 */
#include "libbb.h"
#include "unicode.h"


/* This is a NOEXEC applet. Be very careful! */


#if ENABLE_FTPD
/* ftpd uses ls, and without timestamps Mozilla won't understand
 * ftpd's LIST output.
 */
# undef CONFIG_FEATURE_LS_TIMESTAMPS
# undef ENABLE_FEATURE_LS_TIMESTAMPS
# undef IF_FEATURE_LS_TIMESTAMPS
# undef IF_NOT_FEATURE_LS_TIMESTAMPS
# define CONFIG_FEATURE_LS_TIMESTAMPS 1
# define ENABLE_FEATURE_LS_TIMESTAMPS 1
# define IF_FEATURE_LS_TIMESTAMPS(...) __VA_ARGS__
# define IF_NOT_FEATURE_LS_TIMESTAMPS(...)
#endif


enum {

TERMINAL_WIDTH  = 80,           /* use 79 if terminal has linefold bug */
COLUMN_GAP      = 2,            /* includes the file type char */

/* what is the overall style of the listing */
STYLE_COLUMNS   = 1 << 21,      /* fill columns */
STYLE_LONG      = 2 << 21,      /* one record per line, extended info */
STYLE_SINGLE    = 3 << 21,      /* one record per line */
STYLE_MASK      = STYLE_SINGLE,

/* 51306 lrwxrwxrwx  1 root     root         2 May 11 01:43 /bin/view -> vi* */
/* what file information will be listed */
LIST_INO        = 1 << 0,
LIST_BLOCKS     = 1 << 1,
LIST_MODEBITS   = 1 << 2,
LIST_NLINKS     = 1 << 3,
LIST_ID_NAME    = 1 << 4,
LIST_ID_NUMERIC = 1 << 5,
LIST_CONTEXT    = 1 << 6,
LIST_SIZE       = 1 << 7,
//LIST_DEV        = 1 << 8, - unused, synonym to LIST_SIZE
LIST_DATE_TIME  = 1 << 9,
LIST_FULLTIME   = 1 << 10,
LIST_FILENAME   = 1 << 11,
LIST_SYMLINK    = 1 << 12,
LIST_FILETYPE   = 1 << 13,
LIST_EXEC       = 1 << 14,
LIST_MASK       = (LIST_EXEC << 1) - 1,

/* what files will be displayed */
DISP_DIRNAME    = 1 << 15,      /* 2 or more items? label directories */
DISP_HIDDEN     = 1 << 16,      /* show filenames starting with . */
DISP_DOT        = 1 << 17,      /* show . and .. */
DISP_NOLIST     = 1 << 18,      /* show directory as itself, not contents */
DISP_RECURSIVE  = 1 << 19,      /* show directory and everything below it */
DISP_ROWS       = 1 << 20,      /* print across rows */
DISP_MASK       = ((DISP_ROWS << 1) - 1) & ~(DISP_DIRNAME - 1),

/* how will the files be sorted (CONFIG_FEATURE_LS_SORTFILES) */
SORT_FORWARD    = 0,            /* sort in reverse order */
SORT_REVERSE    = 1 << 27,      /* sort in reverse order */

SORT_NAME       = 0,            /* sort by file name */
SORT_SIZE       = 1 << 28,      /* sort by file size */
SORT_ATIME      = 2 << 28,      /* sort by last access time */
SORT_CTIME      = 3 << 28,      /* sort by last change time */
SORT_MTIME      = 4 << 28,      /* sort by last modification time */
SORT_VERSION    = 5 << 28,      /* sort by version */
SORT_EXT        = 6 << 28,      /* sort by file name extension */
SORT_DIR        = 7 << 28,      /* sort by file or directory */
SORT_MASK       = (7 << 28) * ENABLE_FEATURE_LS_SORTFILES,

/* which of the three times will be used */
TIME_CHANGE     = (1 << 23) * ENABLE_FEATURE_LS_TIMESTAMPS,
TIME_ACCESS     = (1 << 24) * ENABLE_FEATURE_LS_TIMESTAMPS,
TIME_MASK       = (3 << 23) * ENABLE_FEATURE_LS_TIMESTAMPS,

FOLLOW_LINKS    = (1 << 25) * ENABLE_FEATURE_LS_FOLLOWLINKS,

LS_DISP_HR      = (1 << 26) * ENABLE_FEATURE_HUMAN_READABLE,

LIST_SHORT      = LIST_FILENAME,
LIST_LONG       = LIST_MODEBITS | LIST_NLINKS | LIST_ID_NAME | LIST_SIZE | \
                  LIST_DATE_TIME | LIST_FILENAME | LIST_SYMLINK,

SPLIT_DIR       = 1,
SPLIT_FILE      = 0,
SPLIT_SUBDIR    = 2,

};

/* "[-]Cadil1", POSIX mandated options, busybox always supports */
/* "[-]gnsx", POSIX non-mandated options, busybox always supports */
/* "[-]Q" GNU option? busybox always supports */
/* "[-]Ak" GNU options, busybox always supports */
/* "[-]FLRctur", POSIX mandated options, busybox optionally supports */
/* "[-]p", POSIX non-mandated options, busybox optionally supports */
/* "[-]SXvThw", GNU options, busybox optionally supports */
/* "[-]K", SELinux mandated options, busybox optionally supports */
/* "[-]e", I think we made this one up */
static const char ls_options[] ALIGN1 =
	"Cadil1gnsxQAk" /* 13 opts, total 13 */
	IF_FEATURE_LS_TIMESTAMPS("cetu") /* 4, 17 */
	IF_FEATURE_LS_SORTFILES("SXrv")  /* 4, 21 */
	IF_FEATURE_LS_FILETYPES("Fp")    /* 2, 23 */
	IF_FEATURE_LS_FOLLOWLINKS("L")   /* 1, 24 */
	IF_FEATURE_LS_RECURSIVE("R")     /* 1, 25 */
	IF_FEATURE_HUMAN_READABLE("h")   /* 1, 26 */
	IF_SELINUX("KZ") /* 2, 28 */
	IF_FEATURE_AUTOWIDTH("T:w:") /* 2, 30 */
	;
enum {
	//OPT_C = (1 << 0),
	//OPT_a = (1 << 1),
	//OPT_d = (1 << 2),
	//OPT_i = (1 << 3),
	//OPT_l = (1 << 4),
	//OPT_1 = (1 << 5),
	OPT_g = (1 << 6),
	//OPT_n = (1 << 7),
	//OPT_s = (1 << 8),
	//OPT_x = (1 << 9),
	OPT_Q = (1 << 10),
	//OPT_A = (1 << 11),
	//OPT_k = (1 << 12),
	OPTBIT_color = 13
		+ 4 * ENABLE_FEATURE_LS_TIMESTAMPS
		+ 4 * ENABLE_FEATURE_LS_SORTFILES
		+ 2 * ENABLE_FEATURE_LS_FILETYPES
		+ 1 * ENABLE_FEATURE_LS_FOLLOWLINKS
		+ 1 * ENABLE_FEATURE_LS_RECURSIVE
		+ 1 * ENABLE_FEATURE_HUMAN_READABLE
		+ 2 * ENABLE_SELINUX
		+ 2 * ENABLE_FEATURE_AUTOWIDTH,
	OPT_color = 1 << OPTBIT_color,
};

enum {
	LIST_MASK_TRIGGER	= 0,
	STYLE_MASK_TRIGGER	= STYLE_MASK,
	DISP_MASK_TRIGGER	= DISP_ROWS,
	SORT_MASK_TRIGGER	= SORT_MASK,
};

/* TODO: simple toggles may be stored as OPT_xxx bits instead */
static const unsigned opt_flags[] = {
	LIST_SHORT | STYLE_COLUMNS, /* C */
	DISP_HIDDEN | DISP_DOT,     /* a */
	DISP_NOLIST,                /* d */
	LIST_INO,                   /* i */
	LIST_LONG | STYLE_LONG,     /* l - remember LS_DISP_HR in mask! */
	LIST_SHORT | STYLE_SINGLE,  /* 1 */
	0,                          /* g (don't show owner) - handled via OPT_g */
	LIST_ID_NUMERIC,            /* n */
	LIST_BLOCKS,                /* s */
	DISP_ROWS,                  /* x */
	0,                          /* Q (quote filename) - handled via OPT_Q */
	DISP_HIDDEN,                /* A */
	ENABLE_SELINUX * LIST_CONTEXT, /* k (ignored if !SELINUX) */
#if ENABLE_FEATURE_LS_TIMESTAMPS
	TIME_CHANGE | (ENABLE_FEATURE_LS_SORTFILES * SORT_CTIME),   /* c */
	LIST_FULLTIME,              /* e */
	ENABLE_FEATURE_LS_SORTFILES * SORT_MTIME,   /* t */
	TIME_ACCESS | (ENABLE_FEATURE_LS_SORTFILES * SORT_ATIME),   /* u */
#endif
#if ENABLE_FEATURE_LS_SORTFILES
	SORT_SIZE,                  /* S */
	SORT_EXT,                   /* X */
	SORT_REVERSE,               /* r */
	SORT_VERSION,               /* v */
#endif
#if ENABLE_FEATURE_LS_FILETYPES
	LIST_FILETYPE | LIST_EXEC,  /* F */
	LIST_FILETYPE,              /* p */
#endif
#if ENABLE_FEATURE_LS_FOLLOWLINKS
	FOLLOW_LINKS,               /* L */
#endif
#if ENABLE_FEATURE_LS_RECURSIVE
	DISP_RECURSIVE,             /* R */
#endif
#if ENABLE_FEATURE_HUMAN_READABLE
	LS_DISP_HR,                 /* h */
#endif
#if ENABLE_SELINUX
	LIST_MODEBITS|LIST_NLINKS|LIST_CONTEXT|LIST_SIZE|LIST_DATE_TIME, /* K */
#endif
#if ENABLE_SELINUX
	LIST_MODEBITS|LIST_ID_NAME|LIST_CONTEXT, /* Z */
#endif
	(1U<<31)
	/* options after Z are not processed through opt_flags:
	 * T, w - ignored
	 */
};


/*
 * a directory entry and its stat info are stored here
 */
struct dnode {
	const char *name;       /* the dir entry name */
	const char *fullname;   /* the dir entry name */
	struct dnode *next;     /* point at the next node */
	smallint fname_allocated;
	struct stat dstat;      /* the file stat info */
	IF_SELINUX(security_context_t sid;)
};

struct globals {
#if ENABLE_FEATURE_LS_COLOR
	smallint show_color;
#endif
	smallint exit_code;
	unsigned all_fmt;
#if ENABLE_FEATURE_AUTOWIDTH
	unsigned tabstops; // = COLUMN_GAP;
	unsigned terminal_width; // = TERMINAL_WIDTH;
#endif
#if ENABLE_FEATURE_LS_TIMESTAMPS
	/* Do time() just once. Saves one syscall per file for "ls -l" */
	time_t current_time_t;
#endif
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#if ENABLE_FEATURE_LS_COLOR
# define show_color     (G.show_color    )
#else
enum { show_color = 0 };
#endif
#define exit_code       (G.exit_code     )
#define all_fmt         (G.all_fmt       )
#if ENABLE_FEATURE_AUTOWIDTH
# define tabstops       (G.tabstops      )
# define terminal_width (G.terminal_width)
#else
enum {
	tabstops = COLUMN_GAP,
	terminal_width = TERMINAL_WIDTH,
};
#endif
#define current_time_t (G.current_time_t)
#define INIT_G() do { \
	/* we have to zero it out because of NOEXEC */ \
	memset(&G, 0, sizeof(G)); \
	IF_FEATURE_AUTOWIDTH(tabstops = COLUMN_GAP;) \
	IF_FEATURE_AUTOWIDTH(terminal_width = TERMINAL_WIDTH;) \
	IF_FEATURE_LS_TIMESTAMPS(time(&current_time_t);) \
} while (0)


static struct dnode *my_stat(const char *fullname, const char *name, int force_follow)
{
	struct stat dstat;
	struct dnode *cur;
	IF_SELINUX(security_context_t sid = NULL;)

	if ((all_fmt & FOLLOW_LINKS) || force_follow) {
#if ENABLE_SELINUX
		if (is_selinux_enabled())  {
			 getfilecon(fullname, &sid);
		}
#endif
		if (stat(fullname, &dstat)) {
			bb_simple_perror_msg(fullname);
			exit_code = EXIT_FAILURE;
			return 0;
		}
	} else {
#if ENABLE_SELINUX
		if (is_selinux_enabled()) {
			lgetfilecon(fullname, &sid);
		}
#endif
		if (lstat(fullname, &dstat)) {
			bb_simple_perror_msg(fullname);
			exit_code = EXIT_FAILURE;
			return 0;
		}
	}

	cur = xmalloc(sizeof(*cur));
	cur->fullname = fullname;
	cur->name = name;
	cur->dstat = dstat;
	IF_SELINUX(cur->sid = sid;)
	return cur;
}

/* FYI type values: 1:fifo 2:char 4:dir 6:blk 8:file 10:link 12:socket
 * (various wacky OSes: 13:Sun door 14:BSD whiteout 5:XENIX named file
 *  3/7:multiplexed char/block device)
 * and we use 0 for unknown and 15 for executables (see below) */
#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])
#define APPCHAR(mode)   ("\0|\0\0/\0\0\0\0\0@\0=\0\0\0" [TYPEINDEX(mode)])
/* 036 black foreground              050 black background
   037 red foreground                051 red background
   040 green foreground              052 green background
   041 brown foreground              053 brown background
   042 blue foreground               054 blue background
   043 magenta (purple) foreground   055 magenta background
   044 cyan (light blue) foreground  056 cyan background
   045 gray foreground               057 white background
*/
#define COLOR(mode) ( \
	/*un  fi  chr     dir     blk     file    link    sock        exe */ \
	"\037\043\043\045\042\045\043\043\000\045\044\045\043\045\045\040" \
	[TYPEINDEX(mode)])
/* Select normal (0) [actually "reset all"] or bold (1)
 * (other attributes are 2:dim 4:underline 5:blink 7:reverse,
 *  let's use 7 for "impossible" types, just for fun)
 * Note: coreutils 6.9 uses inverted red for setuid binaries.
 */
#define ATTR(mode) ( \
	/*un fi chr   dir   blk   file  link  sock     exe */ \
	"\01\00\01\07\01\07\01\07\00\07\01\07\01\07\07\01" \
	[TYPEINDEX(mode)])

#if ENABLE_FEATURE_LS_COLOR
/* mode of zero is interpreted as "unknown" (stat failed) */
static char fgcolor(mode_t mode)
{
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return COLOR(0xF000);	/* File is executable ... */
	return COLOR(mode);
}
static char bold(mode_t mode)
{
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return ATTR(0xF000);	/* File is executable ... */
	return ATTR(mode);
}
#endif

#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
static char append_char(mode_t mode)
{
	if (!(all_fmt & LIST_FILETYPE))
		return '\0';
	if (S_ISDIR(mode))
		return '/';
	if (!(all_fmt & LIST_EXEC))
		return '\0';
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return '*';
	return APPCHAR(mode);
}
#endif

static unsigned count_dirs(struct dnode **dn, int which)
{
	unsigned dirs, all;

	if (!dn)
		return 0;

	dirs = all = 0;
	for (; *dn; dn++) {
		const char *name;

		all++;
		if (!S_ISDIR((*dn)->dstat.st_mode))
			continue;
		name = (*dn)->name;
		if (which != SPLIT_SUBDIR /* if not requested to skip . / .. */
		 /* or if it's not . or .. */
		 || name[0] != '.' || (name[1] && (name[1] != '.' || name[2]))
		) {
			dirs++;
		}
	}
	return which != SPLIT_FILE ? dirs : all - dirs;
}

/* get memory to hold an array of pointers */
static struct dnode **dnalloc(unsigned num)
{
	if (num < 1)
		return NULL;

	num++; /* so that we have terminating NULL */
	return xzalloc(num * sizeof(struct dnode *));
}

#if ENABLE_FEATURE_LS_RECURSIVE
static void dfree(struct dnode **dnp)
{
	unsigned i;

	if (dnp == NULL)
		return;

	for (i = 0; dnp[i]; i++) {
		struct dnode *cur = dnp[i];
		if (cur->fname_allocated)
			free((char*)cur->fullname);
		free(cur);
	}
	free(dnp);
}
#else
#define dfree(...) ((void)0)
#endif

/* Returns NULL-terminated malloced vector of pointers (or NULL) */
static struct dnode **splitdnarray(struct dnode **dn, int which)
{
	unsigned dncnt, d;
	struct dnode **dnp;

	if (dn == NULL)
		return NULL;

	/* count how many dirs or files there are */
	dncnt = count_dirs(dn, which);

	/* allocate a file array and a dir array */
	dnp = dnalloc(dncnt);

	/* copy the entrys into the file or dir array */
	for (d = 0; *dn; dn++) {
		if (S_ISDIR((*dn)->dstat.st_mode)) {
			const char *name;

			if (!(which & (SPLIT_DIR|SPLIT_SUBDIR)))
				continue;
			name = (*dn)->name;
			if ((which & SPLIT_DIR)
			 || name[0]!='.' || (name[1] && (name[1]!='.' || name[2]))
			) {
				dnp[d++] = *dn;
			}
		} else if (!(which & (SPLIT_DIR|SPLIT_SUBDIR))) {
			dnp[d++] = *dn;
		}
	}
	return dnp;
}

#if ENABLE_FEATURE_LS_SORTFILES
static int sortcmp(const void *a, const void *b)
{
	struct dnode *d1 = *(struct dnode **)a;
	struct dnode *d2 = *(struct dnode **)b;
	unsigned sort_opts = all_fmt & SORT_MASK;
	off_t dif;

	dif = 0; /* assume SORT_NAME */
	// TODO: use pre-initialized function pointer
	// instead of branch forest
	if (sort_opts == SORT_SIZE) {
		dif = (d2->dstat.st_size - d1->dstat.st_size);
	} else if (sort_opts == SORT_ATIME) {
		dif = (d2->dstat.st_atime - d1->dstat.st_atime);
	} else if (sort_opts == SORT_CTIME) {
		dif = (d2->dstat.st_ctime - d1->dstat.st_ctime);
	} else if (sort_opts == SORT_MTIME) {
		dif = (d2->dstat.st_mtime - d1->dstat.st_mtime);
	} else if (sort_opts == SORT_DIR) {
		dif = S_ISDIR(d2->dstat.st_mode) - S_ISDIR(d1->dstat.st_mode);
		/* } else if (sort_opts == SORT_VERSION) { */
		/* } else if (sort_opts == SORT_EXT) { */
	}
	if (dif == 0) {
		/* sort by name, or tie_breaker for other sorts */
		if (ENABLE_LOCALE_SUPPORT)
			dif = strcoll(d1->name, d2->name);
		else
			dif = strcmp(d1->name, d2->name);
	}

	/* Make dif fit into an int */
	if (sizeof(dif) > sizeof(int)) {
		enum { BITS_TO_SHIFT = 8 * (sizeof(dif) - sizeof(int)) };
		/* shift leaving only "int" worth of bits */
		if (dif != 0) {
			dif = 1 | (int)((uoff_t)dif >> BITS_TO_SHIFT);
		}
	}

	return (all_fmt & SORT_REVERSE) ? -(int)dif : (int)dif;
}

static void dnsort(struct dnode **dn, int size)
{
	qsort(dn, size, sizeof(*dn), sortcmp);
}
#else
#define dnsort(dn, size) ((void)0)
#endif


static unsigned calc_name_len(const char *name)
{
	unsigned len;
	uni_stat_t uni_stat;

	// TODO: quote tab as \t, etc, if -Q
	name = printable_string(&uni_stat, name);

	if (!(option_mask32 & OPT_Q)) {
		return uni_stat.unicode_width;
	}

	len = 2 + uni_stat.unicode_width;
	while (*name) {
		if (*name == '"' || *name == '\\') {
			len++;
		}
		name++;
	}
	return len;
}


/* Return the number of used columns.
 * Note that only STYLE_COLUMNS uses return value.
 * STYLE_SINGLE and STYLE_LONG don't care.
 * coreutils 7.2 also supports:
 * ls -b (--escape) = octal escapes (although it doesn't look like working)
 * ls -N (--literal) = not escape at all
 */
static unsigned print_name(const char *name)
{
	unsigned len;
	uni_stat_t uni_stat;

	// TODO: quote tab as \t, etc, if -Q
	name = printable_string(&uni_stat, name);

	if (!(option_mask32 & OPT_Q)) {
		fputs(name, stdout);
		return uni_stat.unicode_width;
	}

	len = 2 + uni_stat.unicode_width;
	putchar('"');
	while (*name) {
		if (*name == '"' || *name == '\\') {
			putchar('\\');
			len++;
		}
		putchar(*name++);
	}
	putchar('"');
	return len;
}

/* Return the number of used columns.
 * Note that only STYLE_COLUMNS uses return value,
 * STYLE_SINGLE and STYLE_LONG don't care.
 */
static NOINLINE unsigned list_single(const struct dnode *dn)
{
	unsigned column = 0;
	char *lpath = lpath; /* for compiler */
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
	struct stat info;
	char append;
#endif

	/* Never happens:
	if (dn->fullname == NULL)
		return 0;
	*/

#if ENABLE_FEATURE_LS_FILETYPES
	append = append_char(dn->dstat.st_mode);
#endif

	/* Do readlink early, so that if it fails, error message
	 * does not appear *inside* the "ls -l" line */
	if (all_fmt & LIST_SYMLINK)
		if (S_ISLNK(dn->dstat.st_mode))
			lpath = xmalloc_readlink_or_warn(dn->fullname);

	if (all_fmt & LIST_INO)
		column += printf("%7llu ", (long long) dn->dstat.st_ino);
	if (all_fmt & LIST_BLOCKS)
		column += printf("%4"OFF_FMT"u ", (off_t) (dn->dstat.st_blocks >> 1));
	if (all_fmt & LIST_MODEBITS)
		column += printf("%-10s ", (char *) bb_mode_string(dn->dstat.st_mode));
	if (all_fmt & LIST_NLINKS)
		column += printf("%4lu ", (long) dn->dstat.st_nlink);
#if ENABLE_FEATURE_LS_USERNAME
	if (all_fmt & LIST_ID_NAME) {
		if (option_mask32 & OPT_g) {
			column += printf("%-8.8s ",
				get_cached_groupname(dn->dstat.st_gid));
		} else {
			column += printf("%-8.8s %-8.8s ",
				get_cached_username(dn->dstat.st_uid),
				get_cached_groupname(dn->dstat.st_gid));
		}
	}
#endif
	if (all_fmt & LIST_ID_NUMERIC) {
		if (option_mask32 & OPT_g)
			column += printf("%-8u ", (int) dn->dstat.st_gid);
		else
			column += printf("%-8u %-8u ",
					(int) dn->dstat.st_uid,
					(int) dn->dstat.st_gid);
	}
	if (all_fmt & (LIST_SIZE /*|LIST_DEV*/ )) {
		if (S_ISBLK(dn->dstat.st_mode) || S_ISCHR(dn->dstat.st_mode)) {
			column += printf("%4u, %3u ",
					(int) major(dn->dstat.st_rdev),
					(int) minor(dn->dstat.st_rdev));
		} else {
			if (all_fmt & LS_DISP_HR) {
				column += printf("%"HUMAN_READABLE_MAX_WIDTH_STR"s ",
					/* print st_size, show one fractional, use suffixes */
					make_human_readable_str(dn->dstat.st_size, 1, 0)
				);
			} else {
				column += printf("%9"OFF_FMT"u ", (off_t) dn->dstat.st_size);
			}
		}
	}
#if ENABLE_FEATURE_LS_TIMESTAMPS
	if (all_fmt & (LIST_FULLTIME|LIST_DATE_TIME)) {
		char *filetime;
		time_t ttime = dn->dstat.st_mtime;
		if (all_fmt & TIME_ACCESS)
			ttime = dn->dstat.st_atime;
		if (all_fmt & TIME_CHANGE)
			ttime = dn->dstat.st_ctime;
		filetime = ctime(&ttime);
		/* filetime's format: "Wed Jun 30 21:49:08 1993\n" */
		if (all_fmt & LIST_FULLTIME)
			column += printf("%.24s ", filetime);
		else { /* LIST_DATE_TIME */
			/* current_time_t ~== time(NULL) */
			time_t age = current_time_t - ttime;
			printf("%.6s ", filetime + 4); /* "Jun 30" */
			if (age < 3600L * 24 * 365 / 2 && age > -15 * 60) {
				/* hh:mm if less than 6 months old */
				printf("%.5s ", filetime + 11);
			} else { /* year. buggy if year > 9999 ;) */
				printf(" %.4s ", filetime + 20);
			}
			column += 13;
		}
	}
#endif
#if ENABLE_SELINUX
	if (all_fmt & LIST_CONTEXT) {
		column += printf("%-32s ", dn->sid ? dn->sid : "unknown");
		freecon(dn->sid);
	}
#endif
	if (all_fmt & LIST_FILENAME) {
#if ENABLE_FEATURE_LS_COLOR
		if (show_color) {
			info.st_mode = 0; /* for fgcolor() */
			lstat(dn->fullname, &info);
			printf("\033[%u;%um", bold(info.st_mode),
					fgcolor(info.st_mode));
		}
#endif
		column += print_name(dn->name);
		if (show_color) {
			printf("\033[0m");
		}
	}
	if (all_fmt & LIST_SYMLINK) {
		if (S_ISLNK(dn->dstat.st_mode) && lpath) {
			printf(" -> ");
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
#if ENABLE_FEATURE_LS_COLOR
			info.st_mode = 0; /* for fgcolor() */
#endif
			if (stat(dn->fullname, &info) == 0) {
				append = append_char(info.st_mode);
			}
#endif
#if ENABLE_FEATURE_LS_COLOR
			if (show_color) {
				printf("\033[%u;%um", bold(info.st_mode),
					   fgcolor(info.st_mode));
			}
#endif
			column += print_name(lpath) + 4;
			if (show_color) {
				printf("\033[0m");
			}
			free(lpath);
		}
	}
#if ENABLE_FEATURE_LS_FILETYPES
	if (all_fmt & LIST_FILETYPE) {
		if (append) {
			putchar(append);
			column++;
		}
	}
#endif

	return column;
}

static void showfiles(struct dnode **dn, unsigned nfiles)
{
	unsigned i, ncols, nrows, row, nc;
	unsigned column = 0;
	unsigned nexttab = 0;
	unsigned column_width = 0; /* used only by STYLE_COLUMNS */

	if (all_fmt & STYLE_LONG) { /* STYLE_LONG or STYLE_SINGLE */
		ncols = 1;
	} else {
		/* find the longest file name, use that as the column width */
		for (i = 0; dn[i]; i++) {
			int len = calc_name_len(dn[i]->name);
			if (column_width < len)
				column_width = len;
		}
		column_width += tabstops +
			IF_SELINUX( ((all_fmt & LIST_CONTEXT) ? 33 : 0) + )
				((all_fmt & LIST_INO) ? 8 : 0) +
				((all_fmt & LIST_BLOCKS) ? 5 : 0);
		ncols = (int) (terminal_width / column_width);
	}

	if (ncols > 1) {
		nrows = nfiles / ncols;
		if (nrows * ncols < nfiles)
			nrows++;                /* round up fractionals */
	} else {
		nrows = nfiles;
		ncols = 1;
	}

	for (row = 0; row < nrows; row++) {
		for (nc = 0; nc < ncols; nc++) {
			/* reach into the array based on the column and row */
			if (all_fmt & DISP_ROWS)
				i = (row * ncols) + nc;	/* display across row */
			else
				i = (nc * nrows) + row;	/* display by column */
			if (i < nfiles) {
				if (column > 0) {
					nexttab -= column;
					printf("%*s", nexttab, "");
					column += nexttab;
				}
				nexttab = column + column_width;
				column += list_single(dn[i]);
			}
		}
		putchar('\n');
		column = 0;
	}
}


#if ENABLE_DESKTOP
/* http://www.opengroup.org/onlinepubs/9699919799/utilities/ls.html
 * If any of the -l, -n, -s options is specified, each list
 * of files within the directory shall be preceded by a
 * status line indicating the number of file system blocks
 * occupied by files in the directory in 512-byte units if
 * the -k option is not specified, or 1024-byte units if the
 * -k option is specified, rounded up to the next integral
 * number of units.
 */
/* by Jorgen Overgaard (jorgen AT antistaten.se) */
static off_t calculate_blocks(struct dnode **dn)
{
	uoff_t blocks = 1;
	if (dn) {
		while (*dn) {
			/* st_blocks is in 512 byte blocks */
			blocks += (*dn)->dstat.st_blocks;
			dn++;
		}
	}

	/* Even though standard says use 512 byte blocks, coreutils use 1k */
	/* Actually, we round up by calculating (blocks + 1) / 2,
	 * "+ 1" was done when we initialized blocks to 1 */
	return blocks >> 1;
}
#endif


static struct dnode **list_dir(const char *, unsigned *);

static void showdirs(struct dnode **dn, int first)
{
	unsigned nfiles;
	unsigned dndirs;
	struct dnode **subdnp;
	struct dnode **dnd;

	/* Never happens:
	if (dn == NULL || ndirs < 1) {
		return;
	}
	*/

	for (; *dn; dn++) {
		if (all_fmt & (DISP_DIRNAME | DISP_RECURSIVE)) {
			if (!first)
				bb_putchar('\n');
			first = 0;
			printf("%s:\n", (*dn)->fullname);
		}
		subdnp = list_dir((*dn)->fullname, &nfiles);
#if ENABLE_DESKTOP
		if ((all_fmt & STYLE_MASK) == STYLE_LONG)
			printf("total %"OFF_FMT"u\n", calculate_blocks(subdnp));
#endif
		if (nfiles > 0) {
			/* list all files at this level */
			dnsort(subdnp, nfiles);
			showfiles(subdnp, nfiles);
			if (ENABLE_FEATURE_LS_RECURSIVE
			 && (all_fmt & DISP_RECURSIVE)
			) {
				/* recursive - list the sub-dirs */
				dnd = splitdnarray(subdnp, SPLIT_SUBDIR);
				dndirs = count_dirs(subdnp, SPLIT_SUBDIR);
				if (dndirs > 0) {
					dnsort(dnd, dndirs);
					showdirs(dnd, 0);
					/* free the array of dnode pointers to the dirs */
					free(dnd);
				}
			}
			/* free the dnodes and the fullname mem */
			dfree(subdnp);
		}
	}
}


/* Returns NULL-terminated malloced vector of pointers (or NULL) */
static struct dnode **list_dir(const char *path, unsigned *nfiles_p)
{
	struct dnode *dn, *cur, **dnp;
	struct dirent *entry;
	DIR *dir;
	unsigned i, nfiles;

	/* Never happens:
	if (path == NULL)
		return NULL;
	*/

	*nfiles_p = 0;
	dir = warn_opendir(path);
	if (dir == NULL) {
		exit_code = EXIT_FAILURE;
		return NULL;	/* could not open the dir */
	}
	dn = NULL;
	nfiles = 0;
	while ((entry = readdir(dir)) != NULL) {
		char *fullname;

		/* are we going to list the file- it may be . or .. or a hidden file */
		if (entry->d_name[0] == '.') {
			if ((!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2]))
			 && !(all_fmt & DISP_DOT)
			) {
				continue;
			}
			if (!(all_fmt & DISP_HIDDEN))
				continue;
		}
		fullname = concat_path_file(path, entry->d_name);
		cur = my_stat(fullname, bb_basename(fullname), 0);
		if (!cur) {
			free(fullname);
			continue;
		}
		cur->fname_allocated = 1;
		cur->next = dn;
		dn = cur;
		nfiles++;
	}
	closedir(dir);

	if (dn == NULL)
		return NULL;

	/* now that we know how many files there are
	 * allocate memory for an array to hold dnode pointers
	 */
	*nfiles_p = nfiles;
	dnp = dnalloc(nfiles);
	for (i = 0; /* i < nfiles - detected via !dn below */; i++) {
		dnp[i] = dn;	/* save pointer to node in array */
		dn = dn->next;
		if (!dn)
			break;
	}

	return dnp;
}


int ls_main(int argc UNUSED_PARAM, char **argv)
{
	struct dnode **dnd;
	struct dnode **dnf;
	struct dnode **dnp;
	struct dnode *dn;
	struct dnode *cur;
	unsigned opt;
	unsigned nfiles;
	unsigned dnfiles;
	unsigned dndirs;
	unsigned i;
#if ENABLE_FEATURE_LS_COLOR
	/* colored LS support by JaWi, janwillem.janssen@lxtreme.nl */
	/* coreutils 6.10:
	 * # ls --color=BOGUS
	 * ls: invalid argument 'BOGUS' for '--color'
	 * Valid arguments are:
	 * 'always', 'yes', 'force'
	 * 'never', 'no', 'none'
	 * 'auto', 'tty', 'if-tty'
	 * (and substrings: "--color=alwa" work too)
	 */
	static const char ls_longopts[] ALIGN1 =
		"color\0" Optional_argument "\xff"; /* no short equivalent */
	static const char color_str[] ALIGN1 =
		"always\0""yes\0""force\0"
		"auto\0""tty\0""if-tty\0";
	/* need to initialize since --color has _an optional_ argument */
	const char *color_opt = color_str; /* "always" */
#endif

	INIT_G();

	init_unicode();

	all_fmt = LIST_SHORT |
		(ENABLE_FEATURE_LS_SORTFILES * (SORT_NAME | SORT_FORWARD));

#if ENABLE_FEATURE_AUTOWIDTH
	/* obtain the terminal width */
	get_terminal_width_height(STDIN_FILENO, &terminal_width, NULL);
	/* go one less... */
	terminal_width--;
#endif

	/* process options */
	IF_FEATURE_LS_COLOR(applet_long_options = ls_longopts;)
#if ENABLE_FEATURE_AUTOWIDTH
	opt_complementary = "T+:w+"; /* -T N, -w N */
	opt = getopt32(argv, ls_options, &tabstops, &terminal_width
				IF_FEATURE_LS_COLOR(, &color_opt));
#else
	opt = getopt32(argv, ls_options IF_FEATURE_LS_COLOR(, &color_opt));
#endif
	for (i = 0; opt_flags[i] != (1U<<31); i++) {
		if (opt & (1 << i)) {
			unsigned flags = opt_flags[i];

			if (flags & LIST_MASK_TRIGGER)
				all_fmt &= ~LIST_MASK;
			if (flags & STYLE_MASK_TRIGGER)
				all_fmt &= ~STYLE_MASK;
			if (flags & SORT_MASK_TRIGGER)
				all_fmt &= ~SORT_MASK;
			if (flags & DISP_MASK_TRIGGER)
				all_fmt &= ~DISP_MASK;
			if (flags & TIME_MASK)
				all_fmt &= ~TIME_MASK;
			if (flags & LIST_CONTEXT)
				all_fmt |= STYLE_SINGLE;
			/* huh?? opt cannot be 'l' */
			//if (LS_DISP_HR && opt == 'l')
			//	all_fmt &= ~LS_DISP_HR;
			all_fmt |= flags;
		}
	}

#if ENABLE_FEATURE_LS_COLOR
	/* find color bit value - last position for short getopt */
	if (ENABLE_FEATURE_LS_COLOR_IS_DEFAULT && isatty(STDOUT_FILENO)) {
		char *p = getenv("LS_COLORS");
		/* LS_COLORS is unset, or (not empty && not "none") ? */
		if (!p || (p[0] && strcmp(p, "none") != 0))
			show_color = 1;
	}
	if (opt & OPT_color) {
		if (color_opt[0] == 'n')
			show_color = 0;
		else switch (index_in_substrings(color_str, color_opt)) {
		case 3:
		case 4:
		case 5:
			if (isatty(STDOUT_FILENO)) {
		case 0:
		case 1:
		case 2:
				show_color = 1;
			}
		}
	}
#endif

	/* sort out which command line options take precedence */
	if (ENABLE_FEATURE_LS_RECURSIVE && (all_fmt & DISP_NOLIST))
		all_fmt &= ~DISP_RECURSIVE;	/* no recurse if listing only dir */
	if (ENABLE_FEATURE_LS_TIMESTAMPS && ENABLE_FEATURE_LS_SORTFILES) {
		if (all_fmt & TIME_CHANGE)
			all_fmt = (all_fmt & ~SORT_MASK) | SORT_CTIME;
		if (all_fmt & TIME_ACCESS)
			all_fmt = (all_fmt & ~SORT_MASK) | SORT_ATIME;
	}
	if ((all_fmt & STYLE_MASK) != STYLE_LONG) /* only for long list */
		all_fmt &= ~(LIST_ID_NUMERIC|LIST_FULLTIME|LIST_ID_NAME|LIST_ID_NUMERIC);
	if (ENABLE_FEATURE_LS_USERNAME)
		if ((all_fmt & STYLE_MASK) == STYLE_LONG && (all_fmt & LIST_ID_NUMERIC))
			all_fmt &= ~LIST_ID_NAME; /* don't list names if numeric uid */

	/* choose a display format */
	if (!(all_fmt & STYLE_MASK))
		all_fmt |= (isatty(STDOUT_FILENO) ? STYLE_COLUMNS : STYLE_SINGLE);

	argv += optind;
	if (!argv[0])
		*--argv = (char*)".";

	if (argv[1])
		all_fmt |= DISP_DIRNAME; /* 2 or more items? label directories */

	/* stuff the command line file names into a dnode array */
	dn = NULL;
	nfiles = 0;
	do {
		/* NB: follow links on command line unless -l or -s */
		cur = my_stat(*argv, *argv, !(all_fmt & (STYLE_LONG|LIST_BLOCKS)));
		argv++;
		if (!cur)
			continue;
		cur->fname_allocated = 0;
		cur->next = dn;
		dn = cur;
		nfiles++;
	} while (*argv);

	/* nfiles _may_ be 0 here - try "ls doesnt_exist" */
	if (nfiles == 0)
		return exit_code;

	/* now that we know how many files there are
	 * allocate memory for an array to hold dnode pointers
	 */
	dnp = dnalloc(nfiles);
	for (i = 0; /* i < nfiles - detected via !dn below */; i++) {
		dnp[i] = dn;	/* save pointer to node in array */
		dn = dn->next;
		if (!dn)
			break;
	}

	if (all_fmt & DISP_NOLIST) {
		dnsort(dnp, nfiles);
		showfiles(dnp, nfiles);
	} else {
		dnd = splitdnarray(dnp, SPLIT_DIR);
		dnf = splitdnarray(dnp, SPLIT_FILE);
		dndirs = count_dirs(dnp, SPLIT_DIR);
		dnfiles = nfiles - dndirs;
		if (dnfiles > 0) {
			dnsort(dnf, dnfiles);
			showfiles(dnf, dnfiles);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dnf);
		}
		if (dndirs > 0) {
			dnsort(dnd, dndirs);
			showdirs(dnd, dnfiles == 0);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dnd);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		dfree(dnp);
	return exit_code;
}
