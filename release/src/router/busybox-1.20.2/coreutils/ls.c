/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1996 Brian Candler <B.Candler@pobox.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
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

//usage:#define ls_trivial_usage
//usage:	"[-1AaCxd"
//usage:	IF_FEATURE_LS_FOLLOWLINKS("LH")
//usage:	IF_FEATURE_LS_RECURSIVE("R")
//usage:	IF_FEATURE_LS_FILETYPES("Fp") "lins"
//usage:	IF_FEATURE_LS_TIMESTAMPS("e")
//usage:	IF_FEATURE_HUMAN_READABLE("h")
//usage:	IF_FEATURE_LS_SORTFILES("rSXv")
//usage:	IF_FEATURE_LS_TIMESTAMPS("ctu")
//usage:	IF_SELINUX("kKZ") "]"
//usage:	IF_FEATURE_AUTOWIDTH(" [-w WIDTH]") " [FILE]..."
//usage:#define ls_full_usage "\n\n"
//usage:       "List directory contents\n"
//usage:     "\n	-1	One column output"
//usage:     "\n	-a	Include entries which start with ."
//usage:     "\n	-A	Like -a, but exclude . and .."
//usage:     "\n	-C	List by columns"
//usage:     "\n	-x	List by lines"
//usage:     "\n	-d	List directory entries instead of contents"
//usage:	IF_FEATURE_LS_FOLLOWLINKS(
//usage:     "\n	-L	Follow symlinks"
//usage:     "\n	-H	Follow symlinks on command line"
//usage:	)
//usage:	IF_FEATURE_LS_RECURSIVE(
//usage:     "\n	-R	Recurse"
//usage:	)
//usage:	IF_FEATURE_LS_FILETYPES(
//usage:     "\n	-p	Append / to dir entries"
//usage:     "\n	-F	Append indicator (one of */=@|) to entries"
//usage:	)
//usage:     "\n	-l	Long listing format"
//usage:     "\n	-i	List inode numbers"
//usage:     "\n	-n	List numeric UIDs and GIDs instead of names"
//usage:     "\n	-s	List allocated blocks"
//usage:	IF_FEATURE_LS_TIMESTAMPS(
//usage:     "\n	-e	List full date and time"
//usage:	)
//usage:	IF_FEATURE_HUMAN_READABLE(
//usage:     "\n	-h	List sizes in human readable format (1K 243M 2G)"
//usage:	)
//usage:	IF_FEATURE_LS_SORTFILES(
//usage:     "\n	-r	Sort in reverse order"
//usage:     "\n	-S	Sort by size"
//usage:     "\n	-X	Sort by extension"
//usage:     "\n	-v	Sort by version"
//usage:	)
//usage:	IF_FEATURE_LS_TIMESTAMPS(
//usage:     "\n	-c	With -l: sort by ctime"
//usage:     "\n	-t	With -l: sort by mtime"
//usage:     "\n	-u	With -l: sort by atime"
//usage:	)
//usage:	IF_SELINUX(
//usage:     "\n	-k	List security context"
//usage:     "\n	-K	List security context in long format"
//usage:     "\n	-Z	List security context and permission"
//usage:	)
//usage:	IF_FEATURE_AUTOWIDTH(
//usage:     "\n	-w N	Assume the terminal is N columns wide"
//usage:	)
//usage:	IF_FEATURE_LS_COLOR(
//usage:     "\n	--color[={always,never,auto}]	Control coloring"
//usage:	)

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

SPLIT_FILE      = 0,
SPLIT_DIR       = 1,
SPLIT_SUBDIR    = 2,

/* Bits in G.all_fmt: */

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
LIST_DATE_TIME  = 1 << 8,
LIST_FULLTIME   = 1 << 9,
LIST_SYMLINK    = 1 << 10,
LIST_FILETYPE   = 1 << 11, /* show / suffix for dirs */
LIST_CLASSIFY   = 1 << 12, /* requires LIST_FILETYPE, also show *,|,@,= suffixes */
LIST_MASK       = (LIST_CLASSIFY << 1) - 1,

/* what files will be displayed */
DISP_DIRNAME    = 1 << 13,      /* 2 or more items? label directories */
DISP_HIDDEN     = 1 << 14,      /* show filenames starting with . */
DISP_DOT        = 1 << 15,      /* show . and .. */
DISP_NOLIST     = 1 << 16,      /* show directory as itself, not contents */
DISP_RECURSIVE  = 1 << 17,      /* show directory and everything below it */
DISP_ROWS       = 1 << 18,      /* print across rows */
DISP_MASK       = ((DISP_ROWS << 1) - 1) & ~(DISP_DIRNAME - 1),

/* what is the overall style of the listing */
STYLE_COLUMNAR  = 1 << 19,      /* many records per line */
STYLE_LONG      = 2 << 19,      /* one record per line, extended info */
STYLE_SINGLE    = 3 << 19,      /* one record per line */
STYLE_MASK      = STYLE_SINGLE,

/* which of the three times will be used */
TIME_CHANGE     = (1 << 21) * ENABLE_FEATURE_LS_TIMESTAMPS,
TIME_ACCESS     = (2 << 21) * ENABLE_FEATURE_LS_TIMESTAMPS,
TIME_MASK       = (3 << 21) * ENABLE_FEATURE_LS_TIMESTAMPS,

/* how will the files be sorted (CONFIG_FEATURE_LS_SORTFILES) */
SORT_REVERSE    = 1 << 23,

SORT_NAME       = 0,            /* sort by file name */
SORT_SIZE       = 1 << 24,      /* sort by file size */
SORT_ATIME      = 2 << 24,      /* sort by last access time */
SORT_CTIME      = 3 << 24,      /* sort by last change time */
SORT_MTIME      = 4 << 24,      /* sort by last modification time */
SORT_VERSION    = 5 << 24,      /* sort by version */
SORT_EXT        = 6 << 24,      /* sort by file name extension */
SORT_DIR        = 7 << 24,      /* sort by file or directory */
SORT_MASK       = (7 << 24) * ENABLE_FEATURE_LS_SORTFILES,

LIST_LONG       = LIST_MODEBITS | LIST_NLINKS | LIST_ID_NAME | LIST_SIZE | \
                  LIST_DATE_TIME | LIST_SYMLINK,
};

/* -Cadil1  Std options, busybox always supports */
/* -gnsxA   Std options, busybox always supports */
/* -Q       GNU option, busybox always supports */
/* -k       SELinux option, busybox always supports (ignores if !SELinux) */
/*          Std has -k which means "show sizes in kbytes" */
/* -LHRctur Std options, busybox optionally supports */
/* -Fp      Std options, busybox optionally supports */
/* -SXvhTw  GNU options, busybox optionally supports */
/* -T WIDTH Ignored (we don't use tabs on output) */
/* -KZ      SELinux mandated options, busybox optionally supports */
/*          (coreutils 8.4 has no -K, remove it?) */
/* -e       I think we made this one up (looks similar to GNU --full-time) */
/* We already used up all 32 bits, if we need to add more, candidates for removal: */
/* -K, -T, -e (add --full-time instead) */
static const char ls_options[] ALIGN1 =
	"Cadil1gnsxQAk"      /* 13 opts, total 13 */
	IF_FEATURE_LS_TIMESTAMPS("cetu") /* 4, 17 */
	IF_FEATURE_LS_SORTFILES("SXrv")  /* 4, 21 */
	IF_FEATURE_LS_FILETYPES("Fp")    /* 2, 23 */
	IF_FEATURE_LS_RECURSIVE("R")     /* 1, 24 */
	IF_SELINUX("KZ")                 /* 2, 26 */
	IF_FEATURE_LS_FOLLOWLINKS("LH")  /* 2, 28 */
	IF_FEATURE_HUMAN_READABLE("h")   /* 1, 29 */
	IF_FEATURE_AUTOWIDTH("T:w:")     /* 2, 31 */
	/* with --color, we use all 32 bits */;
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

	OPTBIT_c = 13,
	OPTBIT_e,
	OPTBIT_t,
	OPTBIT_u,
	OPTBIT_S = OPTBIT_c + 4 * ENABLE_FEATURE_LS_TIMESTAMPS,
	OPTBIT_X, /* 18 */
	OPTBIT_r,
	OPTBIT_v,
	OPTBIT_F = OPTBIT_S + 4 * ENABLE_FEATURE_LS_SORTFILES,
	OPTBIT_p, /* 22 */
	OPTBIT_R = OPTBIT_F + 2 * ENABLE_FEATURE_LS_FILETYPES,
	OPTBIT_K = OPTBIT_R + 1 * ENABLE_FEATURE_LS_RECURSIVE,
	OPTBIT_Z, /* 25 */
	OPTBIT_L = OPTBIT_K + 2 * ENABLE_SELINUX,
	OPTBIT_H, /* 27 */
	OPTBIT_h = OPTBIT_L + 2 * ENABLE_FEATURE_LS_FOLLOWLINKS,
	OPTBIT_T = OPTBIT_h + 1 * ENABLE_FEATURE_HUMAN_READABLE,
	OPTBIT_w, /* 30 */
	OPTBIT_color = OPTBIT_T + 2 * ENABLE_FEATURE_AUTOWIDTH,

	OPT_c = (1 << OPTBIT_c) * ENABLE_FEATURE_LS_TIMESTAMPS,
	OPT_e = (1 << OPTBIT_e) * ENABLE_FEATURE_LS_TIMESTAMPS,
	OPT_t = (1 << OPTBIT_t) * ENABLE_FEATURE_LS_TIMESTAMPS,
	OPT_u = (1 << OPTBIT_u) * ENABLE_FEATURE_LS_TIMESTAMPS,
	OPT_S = (1 << OPTBIT_S) * ENABLE_FEATURE_LS_SORTFILES,
	OPT_X = (1 << OPTBIT_X) * ENABLE_FEATURE_LS_SORTFILES,
	OPT_r = (1 << OPTBIT_r) * ENABLE_FEATURE_LS_SORTFILES,
	OPT_v = (1 << OPTBIT_v) * ENABLE_FEATURE_LS_SORTFILES,
	OPT_F = (1 << OPTBIT_F) * ENABLE_FEATURE_LS_FILETYPES,
	OPT_p = (1 << OPTBIT_p) * ENABLE_FEATURE_LS_FILETYPES,
	OPT_R = (1 << OPTBIT_R) * ENABLE_FEATURE_LS_RECURSIVE,
	OPT_K = (1 << OPTBIT_K) * ENABLE_SELINUX,
	OPT_Z = (1 << OPTBIT_Z) * ENABLE_SELINUX,
	OPT_L = (1 << OPTBIT_L) * ENABLE_FEATURE_LS_FOLLOWLINKS,
	OPT_H = (1 << OPTBIT_H) * ENABLE_FEATURE_LS_FOLLOWLINKS,
	OPT_h = (1 << OPTBIT_h) * ENABLE_FEATURE_HUMAN_READABLE,
	OPT_T = (1 << OPTBIT_T) * ENABLE_FEATURE_AUTOWIDTH,
	OPT_w = (1 << OPTBIT_w) * ENABLE_FEATURE_AUTOWIDTH,
	OPT_color = (1 << OPTBIT_color) * ENABLE_FEATURE_LS_COLOR,
};

/* TODO: simple toggles may be stored as OPT_xxx bits instead */
static const uint32_t opt_flags[] = {
	STYLE_COLUMNAR,		     /* C */
	DISP_HIDDEN | DISP_DOT,      /* a */
	DISP_NOLIST,                 /* d */
	LIST_INO,                    /* i */
	LIST_LONG | STYLE_LONG,      /* l */
	STYLE_SINGLE,                /* 1 */
	LIST_LONG | STYLE_LONG,      /* g (don't show owner) - handled via OPT_g. assumes l */
	LIST_ID_NUMERIC | LIST_LONG | STYLE_LONG, /* n (assumes l) */
	LIST_BLOCKS,                 /* s */
	DISP_ROWS | STYLE_COLUMNAR,  /* x */
	0,                           /* Q (quote filename) - handled via OPT_Q */
	DISP_HIDDEN,                 /* A */
	ENABLE_SELINUX * (LIST_CONTEXT|STYLE_SINGLE), /* k (ignored if !SELINUX) */
#if ENABLE_FEATURE_LS_TIMESTAMPS
	TIME_CHANGE | (ENABLE_FEATURE_LS_SORTFILES * SORT_CTIME), /* c */
	LIST_FULLTIME,               /* e */
	ENABLE_FEATURE_LS_SORTFILES * SORT_MTIME, /* t */
	TIME_ACCESS | (ENABLE_FEATURE_LS_SORTFILES * SORT_ATIME), /* u */
#endif
#if ENABLE_FEATURE_LS_SORTFILES
	SORT_SIZE,                   /* S */
	SORT_EXT,                    /* X */
	SORT_REVERSE,                /* r */
	SORT_VERSION,                /* v */
#endif
#if ENABLE_FEATURE_LS_FILETYPES
	LIST_FILETYPE | LIST_CLASSIFY, /* F */
	LIST_FILETYPE,               /* p */
#endif
#if ENABLE_FEATURE_LS_RECURSIVE
	DISP_RECURSIVE,              /* R */
#endif
#if ENABLE_SELINUX
	LIST_MODEBITS|LIST_NLINKS|LIST_CONTEXT|LIST_SIZE|LIST_DATE_TIME|STYLE_SINGLE, /* K */
	LIST_MODEBITS|LIST_ID_NAME|LIST_CONTEXT|STYLE_SINGLE, /* Z */
#endif
	(1U << 31)
	/* options after Z are not processed through opt_flags */
};


/*
 * a directory entry and its stat info
 */
struct dnode {
	const char *name;       /* usually basename, but think "ls -l dir/file" */
	const char *fullname;   /* full name (usable for stat etc) */
	struct dnode *dn_next;  /* for linked list */
	IF_SELINUX(security_context_t sid;)
	smallint fname_allocated;

	/* Used to avoid re-doing [l]stat at printout stage
	 * if we already collected needed data in scan stage:
	 */
	mode_t    dn_mode_lstat;   /* obtained with lstat, or 0 */
	mode_t    dn_mode_stat;    /* obtained with stat, or 0 */

//	struct stat dstat;
// struct stat is huge. We don't need it in full.
// At least we don't need st_dev and st_blksize,
// but there are invisible fields as well
// (such as nanosecond-resolution timespamps)
// and padding, which we also don't want to store.
// We also can pre-parse dev_t dn_rdev (in glibc, it's huge).
// On 32-bit uclibc: dnode size went from 112 to 84 bytes.
//
	/* Same names as in struct stat, but with dn_ instead of st_ pfx: */
	mode_t    dn_mode; /* obtained with lstat OR stat, depending on -L etc */
	off_t     dn_size;
#if ENABLE_FEATURE_LS_TIMESTAMPS || ENABLE_FEATURE_LS_SORTFILES
	time_t    dn_atime;
	time_t    dn_mtime;
	time_t    dn_ctime;
#endif
	ino_t     dn_ino;
	blkcnt_t  dn_blocks;
	nlink_t   dn_nlink;
	uid_t     dn_uid;
	gid_t     dn_gid;
	int       dn_rdev_maj;
	int       dn_rdev_min;
//	dev_t     dn_dev;
//	blksize_t dn_blksize;
};

struct globals {
#if ENABLE_FEATURE_LS_COLOR
	smallint show_color;
# define G_show_color (G.show_color)
#else
# define G_show_color 0
#endif
	smallint exit_code;
	unsigned all_fmt;
#if ENABLE_FEATURE_AUTOWIDTH
	unsigned terminal_width;
# define G_terminal_width (G.terminal_width)
#else
# define G_terminal_width TERMINAL_WIDTH
#endif
#if ENABLE_FEATURE_LS_TIMESTAMPS
	/* Do time() just once. Saves one syscall per file for "ls -l" */
	time_t current_time_t;
#endif
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { \
	/* we have to zero it out because of NOEXEC */ \
	memset(&G, 0, sizeof(G)); \
	IF_FEATURE_AUTOWIDTH(G_terminal_width = TERMINAL_WIDTH;) \
	IF_FEATURE_LS_TIMESTAMPS(time(&G.current_time_t);) \
} while (0)


/*** Output code ***/


/* FYI type values: 1:fifo 2:char 4:dir 6:blk 8:file 10:link 12:socket
 * (various wacky OSes: 13:Sun door 14:BSD whiteout 5:XENIX named file
 *  3/7:multiplexed char/block device)
 * and we use 0 for unknown and 15 for executables (see below) */
#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
/*                       un  fi chr -   dir -  blk  -  file -  link - sock -   - exe */
#define APPCHAR(mode)   ("\0""|""\0""\0""/""\0""\0""\0""\0""\0""@""\0""=""\0""\0""\0" [TYPEINDEX(mode)])
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
	/*un  fi  chr  -  dir  -  blk  -  file -  link -  sock -   -  exe */ \
	"\037\043\043\045\042\045\043\043\000\045\044\045\043\045\045\040" \
	[TYPEINDEX(mode)])
/* Select normal (0) [actually "reset all"] or bold (1)
 * (other attributes are 2:dim 4:underline 5:blink 7:reverse,
 *  let's use 7 for "impossible" types, just for fun)
 * Note: coreutils 6.9 uses inverted red for setuid binaries.
 */
#define ATTR(mode) ( \
	/*un fi chr - dir - blk - file- link- sock- -  exe */ \
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

#if ENABLE_FEATURE_LS_FILETYPES
static char append_char(mode_t mode)
{
	if (!(G.all_fmt & LIST_FILETYPE))
		return '\0';
	if (S_ISDIR(mode))
		return '/';
	if (!(G.all_fmt & LIST_CLASSIFY))
		return '\0';
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return '*';
	return APPCHAR(mode);
}
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
 * Note that only STYLE_COLUMNAR uses return value.
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
		putchar(*name);
		name++;
	}
	putchar('"');
	return len;
}

/* Return the number of used columns.
 * Note that only STYLE_COLUMNAR uses return value,
 * STYLE_SINGLE and STYLE_LONG don't care.
 */
static NOINLINE unsigned display_single(const struct dnode *dn)
{
	unsigned column = 0;
	char *lpath;
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
	struct stat statbuf;
	char append;
#endif

#if ENABLE_FEATURE_LS_FILETYPES
	append = append_char(dn->dn_mode);
#endif

	/* Do readlink early, so that if it fails, error message
	 * does not appear *inside* the "ls -l" line */
	lpath = NULL;
	if (G.all_fmt & LIST_SYMLINK)
		if (S_ISLNK(dn->dn_mode))
			lpath = xmalloc_readlink_or_warn(dn->fullname);

	if (G.all_fmt & LIST_INO)
		column += printf("%7llu ", (long long) dn->dn_ino);
//TODO: -h should affect -s too:
	if (G.all_fmt & LIST_BLOCKS)
		column += printf("%6"OFF_FMT"u ", (off_t) (dn->dn_blocks >> 1));
	if (G.all_fmt & LIST_MODEBITS)
		column += printf("%-10s ", (char *) bb_mode_string(dn->dn_mode));
	if (G.all_fmt & LIST_NLINKS)
		column += printf("%4lu ", (long) dn->dn_nlink);
	if (G.all_fmt & LIST_ID_NUMERIC) {
		if (option_mask32 & OPT_g)
			column += printf("%-8u ", (int) dn->dn_gid);
		else
			column += printf("%-8u %-8u ",
					(int) dn->dn_uid,
					(int) dn->dn_gid);
	}
#if ENABLE_FEATURE_LS_USERNAME
	else if (G.all_fmt & LIST_ID_NAME) {
		if (option_mask32 & OPT_g) {
			column += printf("%-8.8s ",
				get_cached_groupname(dn->dn_gid));
		} else {
			column += printf("%-8.8s %-8.8s ",
				get_cached_username(dn->dn_uid),
				get_cached_groupname(dn->dn_gid));
		}
	}
#endif
	if (G.all_fmt & LIST_SIZE) {
		if (S_ISBLK(dn->dn_mode) || S_ISCHR(dn->dn_mode)) {
			column += printf("%4u, %3u ",
					dn->dn_rdev_maj,
					dn->dn_rdev_min);
		} else {
			if (option_mask32 & OPT_h) {
				column += printf("%"HUMAN_READABLE_MAX_WIDTH_STR"s ",
					/* print size, show one fractional, use suffixes */
					make_human_readable_str(dn->dn_size, 1, 0)
				);
			} else {
				column += printf("%9"OFF_FMT"u ", dn->dn_size);
			}
		}
	}
#if ENABLE_FEATURE_LS_TIMESTAMPS
	if (G.all_fmt & (LIST_FULLTIME|LIST_DATE_TIME)) {
		char *filetime;
		time_t ttime = dn->dn_mtime;
		if (G.all_fmt & TIME_ACCESS)
			ttime = dn->dn_atime;
		if (G.all_fmt & TIME_CHANGE)
			ttime = dn->dn_ctime;
		filetime = ctime(&ttime);
		/* filetime's format: "Wed Jun 30 21:49:08 1993\n" */
		if (G.all_fmt & LIST_FULLTIME) { /* -e */
			/* Note: coreutils 8.4 ls --full-time prints:
			 * 2009-07-13 17:49:27.000000000 +0200
			 */
			column += printf("%.24s ", filetime);
		} else { /* LIST_DATE_TIME */
			/* G.current_time_t ~== time(NULL) */
			time_t age = G.current_time_t - ttime;
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
	if (G.all_fmt & LIST_CONTEXT) {
		column += printf("%-32s ", dn->sid ? dn->sid : "unknown");
		freecon(dn->sid);
	}
#endif

#if ENABLE_FEATURE_LS_COLOR
	if (G_show_color) {
		mode_t mode = dn->dn_mode_lstat;
		if (!mode)
			if (lstat(dn->fullname, &statbuf) == 0)
				mode = statbuf.st_mode;
		printf("\033[%u;%um", bold(mode), fgcolor(mode));
	}
#endif
	column += print_name(dn->name);
	if (G_show_color) {
		printf("\033[0m");
	}

	if (lpath) {
		printf(" -> ");
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
		if ((G.all_fmt & LIST_FILETYPE) || G_show_color) {
			mode_t mode = dn->dn_mode_stat;
			if (!mode)
				if (stat(dn->fullname, &statbuf) == 0)
					mode = statbuf.st_mode;
# if ENABLE_FEATURE_LS_FILETYPES
			append = append_char(mode);
# endif
# if ENABLE_FEATURE_LS_COLOR
			if (G_show_color) {
				printf("\033[%u;%um", bold(mode), fgcolor(mode));
			}
# endif
		}
#endif
		column += print_name(lpath) + 4;
		free(lpath);
		if (G_show_color) {
			printf("\033[0m");
		}
	}
#if ENABLE_FEATURE_LS_FILETYPES
	if (G.all_fmt & LIST_FILETYPE) {
		if (append) {
			putchar(append);
			column++;
		}
	}
#endif

	return column;
}

static void display_files(struct dnode **dn, unsigned nfiles)
{
	unsigned i, ncols, nrows, row, nc;
	unsigned column;
	unsigned nexttab;
	unsigned column_width = 0; /* used only by STYLE_COLUMNAR */

	if (G.all_fmt & STYLE_LONG) { /* STYLE_LONG or STYLE_SINGLE */
		ncols = 1;
	} else {
		/* find the longest file name, use that as the column width */
		for (i = 0; dn[i]; i++) {
			int len = calc_name_len(dn[i]->name);
			if (column_width < len)
				column_width = len;
		}
		column_width += 1 +
			IF_SELINUX( ((G.all_fmt & LIST_CONTEXT) ? 33 : 0) + )
				((G.all_fmt & LIST_INO) ? 8 : 0) +
				((G.all_fmt & LIST_BLOCKS) ? 5 : 0);
		ncols = (unsigned)G_terminal_width / column_width;
	}

	if (ncols > 1) {
		nrows = nfiles / ncols;
		if (nrows * ncols < nfiles)
			nrows++;                /* round up fractionals */
	} else {
		nrows = nfiles;
		ncols = 1;
	}

	column = 0;
	nexttab = 0;
	for (row = 0; row < nrows; row++) {
		for (nc = 0; nc < ncols; nc++) {
			/* reach into the array based on the column and row */
			if (G.all_fmt & DISP_ROWS)
				i = (row * ncols) + nc;	/* display across row */
			else
				i = (nc * nrows) + row;	/* display by column */
			if (i < nfiles) {
				if (column > 0) {
					nexttab -= column;
					printf("%*s ", nexttab, "");
					column += nexttab + 1;
				}
				nexttab = column + column_width;
				column += display_single(dn[i]);
			}
		}
		putchar('\n');
		column = 0;
	}
}


/*** Dir scanning code ***/

static struct dnode *my_stat(const char *fullname, const char *name, int force_follow)
{
	struct stat statbuf;
	struct dnode *cur;

	cur = xzalloc(sizeof(*cur));
	cur->fullname = fullname;
	cur->name = name;

	if ((option_mask32 & OPT_L) || force_follow) {
#if ENABLE_SELINUX
		if (is_selinux_enabled())  {
			 getfilecon(fullname, &cur->sid);
		}
#endif
		if (stat(fullname, &statbuf)) {
			bb_simple_perror_msg(fullname);
			G.exit_code = EXIT_FAILURE;
			free(cur);
			return NULL;
		}
		cur->dn_mode_stat = statbuf.st_mode;
	} else {
#if ENABLE_SELINUX
		if (is_selinux_enabled()) {
			lgetfilecon(fullname, &cur->sid);
		}
#endif
		if (lstat(fullname, &statbuf)) {
			bb_simple_perror_msg(fullname);
			G.exit_code = EXIT_FAILURE;
			free(cur);
			return NULL;
		}
		cur->dn_mode_lstat = statbuf.st_mode;
	}

	/* cur->dstat = statbuf: */
	cur->dn_mode   = statbuf.st_mode  ;
	cur->dn_size   = statbuf.st_size  ;
#if ENABLE_FEATURE_LS_TIMESTAMPS || ENABLE_FEATURE_LS_SORTFILES
	cur->dn_atime  = statbuf.st_atime ;
	cur->dn_mtime  = statbuf.st_mtime ;
	cur->dn_ctime  = statbuf.st_ctime ;
#endif
	cur->dn_ino    = statbuf.st_ino   ;
	cur->dn_blocks = statbuf.st_blocks;
	cur->dn_nlink  = statbuf.st_nlink ;
	cur->dn_uid    = statbuf.st_uid   ;
	cur->dn_gid    = statbuf.st_gid   ;
	cur->dn_rdev_maj = major(statbuf.st_rdev);
	cur->dn_rdev_min = minor(statbuf.st_rdev);

	return cur;
}

static unsigned count_dirs(struct dnode **dn, int which)
{
	unsigned dirs, all;

	if (!dn)
		return 0;

	dirs = all = 0;
	for (; *dn; dn++) {
		const char *name;

		all++;
		if (!S_ISDIR((*dn)->dn_mode))
			continue;

		name = (*dn)->name;
		if (which != SPLIT_SUBDIR /* if not requested to skip . / .. */
		 /* or if it's not . or .. */
		 || name[0] != '.'
		 || (name[1] && (name[1] != '.' || name[2]))
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
		if (S_ISDIR((*dn)->dn_mode)) {
			const char *name;

			if (which == SPLIT_FILE)
				continue;

			name = (*dn)->name;
			if ((which & SPLIT_DIR) /* any dir... */
			/* ... or not . or .. */
			 || name[0] != '.'
			 || (name[1] && (name[1] != '.' || name[2]))
			) {
				dnp[d++] = *dn;
			}
		} else
		if (which == SPLIT_FILE) {
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
	unsigned sort_opts = G.all_fmt & SORT_MASK;
	off_t dif;

	dif = 0; /* assume SORT_NAME */
	// TODO: use pre-initialized function pointer
	// instead of branch forest
	if (sort_opts == SORT_SIZE) {
		dif = (d2->dn_size - d1->dn_size);
	} else
	if (sort_opts == SORT_ATIME) {
		dif = (d2->dn_atime - d1->dn_atime);
	} else
	if (sort_opts == SORT_CTIME) {
		dif = (d2->dn_ctime - d1->dn_ctime);
	} else
	if (sort_opts == SORT_MTIME) {
		dif = (d2->dn_mtime - d1->dn_mtime);
	} else
	if (sort_opts == SORT_DIR) {
		dif = S_ISDIR(d2->dn_mode) - S_ISDIR(d1->dn_mode);
	} else
#if defined(HAVE_STRVERSCMP) && HAVE_STRVERSCMP == 1
	if (sort_opts == SORT_VERSION) {
		dif = strverscmp(d1->name, d2->name);
	} else
#endif
	if (sort_opts == SORT_EXT) {
		dif = strcmp(strchrnul(d1->name, '.'), strchrnul(d2->name, '.'));
	}
	if (dif == 0) {
		/* sort by name, use as tie breaker for other sorts */
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

	return (G.all_fmt & SORT_REVERSE) ? -(int)dif : (int)dif;
}

static void dnsort(struct dnode **dn, int size)
{
	qsort(dn, size, sizeof(*dn), sortcmp);
}

static void sort_and_display_files(struct dnode **dn, unsigned nfiles)
{
	dnsort(dn, nfiles);
	display_files(dn, nfiles);
}
#else
# define dnsort(dn, size) ((void)0)
# define sort_and_display_files(dn, nfiles) display_files(dn, nfiles)
#endif

/* Returns NULL-terminated malloced vector of pointers (or NULL) */
static struct dnode **scan_one_dir(const char *path, unsigned *nfiles_p)
{
	struct dnode *dn, *cur, **dnp;
	struct dirent *entry;
	DIR *dir;
	unsigned i, nfiles;

	*nfiles_p = 0;
	dir = warn_opendir(path);
	if (dir == NULL) {
		G.exit_code = EXIT_FAILURE;
		return NULL;	/* could not open the dir */
	}
	dn = NULL;
	nfiles = 0;
	while ((entry = readdir(dir)) != NULL) {
		char *fullname;

		/* are we going to list the file- it may be . or .. or a hidden file */
		if (entry->d_name[0] == '.') {
			if ((!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2]))
			 && !(G.all_fmt & DISP_DOT)
			) {
				continue;
			}
			if (!(G.all_fmt & DISP_HIDDEN))
				continue;
		}
		fullname = concat_path_file(path, entry->d_name);
		cur = my_stat(fullname, bb_basename(fullname), 0);
		if (!cur) {
			free(fullname);
			continue;
		}
		cur->fname_allocated = 1;
		cur->dn_next = dn;
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
		dn = dn->dn_next;
		if (!dn)
			break;
	}

	return dnp;
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
			blocks += (*dn)->dn_blocks;
			dn++;
		}
	}

	/* Even though standard says use 512 byte blocks, coreutils use 1k */
	/* Actually, we round up by calculating (blocks + 1) / 2,
	 * "+ 1" was done when we initialized blocks to 1 */
	return blocks >> 1;
}
#endif

static void scan_and_display_dirs_recur(struct dnode **dn, int first)
{
	unsigned nfiles;
	struct dnode **subdnp;

	for (; *dn; dn++) {
		if (G.all_fmt & (DISP_DIRNAME | DISP_RECURSIVE)) {
			if (!first)
				bb_putchar('\n');
			first = 0;
			printf("%s:\n", (*dn)->fullname);
		}
		subdnp = scan_one_dir((*dn)->fullname, &nfiles);
#if ENABLE_DESKTOP
		if ((G.all_fmt & STYLE_MASK) == STYLE_LONG)
			printf("total %"OFF_FMT"u\n", calculate_blocks(subdnp));
#endif
		if (nfiles > 0) {
			/* list all files at this level */
			sort_and_display_files(subdnp, nfiles);

			if (ENABLE_FEATURE_LS_RECURSIVE
			 && (G.all_fmt & DISP_RECURSIVE)
			) {
				struct dnode **dnd;
				unsigned dndirs;
				/* recursive - list the sub-dirs */
				dnd = splitdnarray(subdnp, SPLIT_SUBDIR);
				dndirs = count_dirs(subdnp, SPLIT_SUBDIR);
				if (dndirs > 0) {
					dnsort(dnd, dndirs);
					scan_and_display_dirs_recur(dnd, 0);
					/* free the array of dnode pointers to the dirs */
					free(dnd);
				}
			}
			/* free the dnodes and the fullname mem */
			dfree(subdnp);
		}
	}
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

	if (ENABLE_FEATURE_LS_SORTFILES)
		G.all_fmt = SORT_NAME;

#if ENABLE_FEATURE_AUTOWIDTH
	/* obtain the terminal width */
	get_terminal_width_height(STDIN_FILENO, &G_terminal_width, NULL);
	/* go one less... */
	G_terminal_width--;
#endif

	/* process options */
	IF_FEATURE_LS_COLOR(applet_long_options = ls_longopts;)
	opt_complementary =
		/* -e implies -l */
		IF_FEATURE_LS_TIMESTAMPS("el")
		/* http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ls.html:
		 * in some pairs of opts, only last one takes effect:
		 */
		IF_FEATURE_LS_TIMESTAMPS(IF_FEATURE_LS_SORTFILES(":t-S:S-t")) /* time/size */
		// ":m-l:l-m" - we don't have -m
		IF_FEATURE_LS_FOLLOWLINKS(":H-L:L-H")
		":C-xl:x-Cl:l-xC" /* bycols/bylines/long */
		":C-1:1-C" /* bycols/oneline */
		":x-1:1-x" /* bylines/oneline (not in SuS, but in GNU coreutils 8.4) */
		IF_FEATURE_LS_TIMESTAMPS(":c-u:u-c") /* mtime/atime */
		/* -w NUM: */
		IF_FEATURE_AUTOWIDTH(":w+");
	opt = getopt32(argv, ls_options
		IF_FEATURE_AUTOWIDTH(, NULL, &G_terminal_width)
		IF_FEATURE_LS_COLOR(, &color_opt)
	);
	for (i = 0; opt_flags[i] != (1U << 31); i++) {
		if (opt & (1 << i)) {
			uint32_t flags = opt_flags[i];

			if (flags & STYLE_MASK)
				G.all_fmt &= ~STYLE_MASK;
			if (flags & SORT_MASK)
				G.all_fmt &= ~SORT_MASK;
			if (flags & TIME_MASK)
				G.all_fmt &= ~TIME_MASK;

			G.all_fmt |= flags;
		}
	}

#if ENABLE_FEATURE_LS_COLOR
	/* set G_show_color = 1/0 */
	if (ENABLE_FEATURE_LS_COLOR_IS_DEFAULT && isatty(STDOUT_FILENO)) {
		char *p = getenv("LS_COLORS");
		/* LS_COLORS is unset, or (not empty && not "none") ? */
		if (!p || (p[0] && strcmp(p, "none") != 0))
			G_show_color = 1;
	}
	if (opt & OPT_color) {
		if (color_opt[0] == 'n')
			G_show_color = 0;
		else switch (index_in_substrings(color_str, color_opt)) {
		case 3:
		case 4:
		case 5:
			if (isatty(STDOUT_FILENO)) {
		case 0:
		case 1:
		case 2:
				G_show_color = 1;
			}
		}
	}
#endif

	/* sort out which command line options take precedence */
	if (ENABLE_FEATURE_LS_RECURSIVE && (G.all_fmt & DISP_NOLIST))
		G.all_fmt &= ~DISP_RECURSIVE;	/* no recurse if listing only dir */
	if (ENABLE_FEATURE_LS_TIMESTAMPS && ENABLE_FEATURE_LS_SORTFILES) {
		if (G.all_fmt & TIME_CHANGE)
			G.all_fmt = (G.all_fmt & ~SORT_MASK) | SORT_CTIME;
		if (G.all_fmt & TIME_ACCESS)
			G.all_fmt = (G.all_fmt & ~SORT_MASK) | SORT_ATIME;
	}
	if ((G.all_fmt & STYLE_MASK) != STYLE_LONG) /* not -l? */
		G.all_fmt &= ~(LIST_ID_NUMERIC|LIST_ID_NAME|LIST_FULLTIME);

	/* choose a display format if one was not already specified by an option */
	if (!(G.all_fmt & STYLE_MASK))
		G.all_fmt |= (isatty(STDOUT_FILENO) ? STYLE_COLUMNAR : STYLE_SINGLE);

	argv += optind;
	if (!argv[0])
		*--argv = (char*)".";

	if (argv[1])
		G.all_fmt |= DISP_DIRNAME; /* 2 or more items? label directories */

	/* stuff the command line file names into a dnode array */
	dn = NULL;
	nfiles = 0;
	do {
		cur = my_stat(*argv, *argv,
			/* follow links on command line unless -l, -s or -F: */
			!((G.all_fmt & STYLE_MASK) == STYLE_LONG
			  || (G.all_fmt & LIST_BLOCKS)
			  || (option_mask32 & OPT_F)
			)
			/* ... or if -H: */
			|| (option_mask32 & OPT_H)
			/* ... or if -L, but my_stat always follows links if -L */
		);
		argv++;
		if (!cur)
			continue;
		/*cur->fname_allocated = 0; - already is */
		cur->dn_next = dn;
		dn = cur;
		nfiles++;
	} while (*argv);

	/* nfiles _may_ be 0 here - try "ls doesnt_exist" */
	if (nfiles == 0)
		return G.exit_code;

	/* now that we know how many files there are
	 * allocate memory for an array to hold dnode pointers
	 */
	dnp = dnalloc(nfiles);
	for (i = 0; /* i < nfiles - detected via !dn below */; i++) {
		dnp[i] = dn;	/* save pointer to node in array */
		dn = dn->dn_next;
		if (!dn)
			break;
	}

	if (G.all_fmt & DISP_NOLIST) {
		sort_and_display_files(dnp, nfiles);
	} else {
		dnd = splitdnarray(dnp, SPLIT_DIR);
		dnf = splitdnarray(dnp, SPLIT_FILE);
		dndirs = count_dirs(dnp, SPLIT_DIR);
		dnfiles = nfiles - dndirs;
		if (dnfiles > 0) {
			sort_and_display_files(dnf, dnfiles);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dnf);
		}
		if (dndirs > 0) {
			dnsort(dnd, dndirs);
			scan_and_display_dirs_recur(dnd, dnfiles == 0);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dnd);
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		dfree(dnp);
	return G.exit_code;
}
