/**************************************************************************
 *   nano.h  --  This file is part of GNU nano.                           *
 *                                                                        *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,  *
 *   2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014, 2015, 2016 Benno Schulenberg                     *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#ifndef NANO_H
#define NANO_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef NEED_XOPEN_SOURCE_EXTENDED
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif
#endif

#ifdef __TANDEM
/* Tandem NonStop Kernel support. */
#include <floss.h>
#define NANO_ROOT_UID 65535
#else
#define NANO_ROOT_UID 0
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <stdarg.h>

/* Suppress warnings for __attribute__((warn_unused_result)). */
#define IGNORE_CALL_RESULT(call) do { if (call) {} } while(0)

/* Macros for flags, indexing each bit in a small array. */
#define FLAGS(flag) flags[((flag) / (sizeof(unsigned) * 8))]
#define FLAGMASK(flag) ((unsigned)1 << ((flag) % (sizeof(unsigned) * 8)))
#define SET(flag) FLAGS(flag) |= FLAGMASK(flag)
#define UNSET(flag) FLAGS(flag) &= ~FLAGMASK(flag)
#define ISSET(flag) ((FLAGS(flag) & FLAGMASK(flag)) != 0)
#define TOGGLE(flag) FLAGS(flag) ^= FLAGMASK(flag)

/* Macros for character allocation and more. */
#define charalloc(howmuch) (char *)nmalloc((howmuch) * sizeof(char))
#define charealloc(ptr, howmuch) (char *)nrealloc(ptr, (howmuch) * sizeof(char))
#define charmove(dest, src, n) memmove(dest, src, (n) * sizeof(char))
#define charset(dest, src, n) memset(dest, src, (n) * sizeof(char))

/* Set a default value for PATH_MAX if there isn't one. */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef USE_SLANG
/* Slang support. */
#include <slcurses.h>
/* Slang curses emulation brain damage, part 3: Slang doesn't define the
 * curses equivalents of the Insert or Delete keys. */
#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC
/* Ncurses support. */
#elif defined(HAVE_NCURSESW_NCURSES_H)
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
/* Curses support. */
#include <curses.h>
#endif /* CURSES_H */

#if defined(NCURSES_VERSION_MAJOR) && (NCURSES_VERSION_MAJOR < 6)
#define USING_OLD_NCURSES yes
#endif

#ifdef ENABLE_NLS
/* Native language support. */
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#define _(string) gettext(string)
#define P_(singular, plural, number) ngettext(singular, plural, number)
#else
#define _(string) (string)
#define P_(singular, plural, number) (number == 1 ? singular : plural)
#endif
#define gettext_noop(string) (string)
#define N_(string) gettext_noop(string)
	/* Mark a string that will be sent to gettext() later. */

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#include <signal.h>
#include <assert.h>

/* If we aren't using ncurses with mouse support, turn the mouse support
 * off, as it's useless then. */
#ifndef NCURSES_MOUSE_VERSION
#define DISABLE_MOUSE 1
#endif

#if defined(DISABLE_WRAPPING) && defined(DISABLE_JUSTIFY)
#define DISABLE_WRAPJUSTIFY 1
#endif

/* Enumeration types. */
typedef enum {
    NIX_FILE, DOS_FILE, MAC_FILE
} file_format;

typedef enum {
    HUSH, MILD, ALERT
} message_type;

typedef enum {
    OVERWRITE, APPEND, PREPEND
} kind_of_writing_type;

typedef enum {
    SOFTMARK, HARDMARK
} mark_type;

typedef enum {
    UPWARD, DOWNWARD
} scroll_dir;

typedef enum {
    CENTERING, FLOWING, STATIONARY
} update_type;

typedef enum {
    ADD, DEL, BACK, CUT, CUT_EOF, REPLACE,
#ifndef DISABLE_WRAPPING
    SPLIT_BEGIN, SPLIT_END,
#endif
#ifndef DISABLE_COMMENT
    COMMENT, UNCOMMENT, PREFLIGHT,
#endif
    JOIN, PASTE, INSERT, ENTER, OTHER
} undo_type;

/* Structure types. */
#ifndef DISABLE_COLOR
typedef struct colortype {
    short fg;
	/* This syntax's foreground color. */
    short bg;
	/* This syntax's background color. */
    bool bright;
	/* Is this color A_BOLD? */
    int pairnum;
	/* The color pair number used for this foreground color and
	 * background color. */
    int attributes;
	/* Pair number and brightness composed into ready-to-use attributes. */
    int rex_flags;
	/* The regex compilation flags (with or without REG_ICASE). */
    char *start_regex;
	/* The start (or all) of the regex string. */
    regex_t *start;
	/* The compiled start (or all) of the regex string. */
    char *end_regex;
	/* The end (if any) of the regex string. */
    regex_t *end;
	/* The compiled end (if any) of the regex string. */
    struct colortype *next;
	/* Next set of colors. */
    int id;
	/* Basic id for assigning to lines later. */
} colortype;

typedef struct regexlisttype {
    char *full_regex;
	/* A regex string to match things that imply a certain syntax. */
    struct regexlisttype *next;
	/* The next regex. */
} regexlisttype;

typedef struct syntaxtype {
    char *name;
	/* The name of this syntax. */
    regexlisttype *extensions;
	/* The list of extensions that this syntax applies to. */
    regexlisttype *headers;
	/* The list of headerlines that this syntax applies to. */
    regexlisttype *magics;
	/* The list of libmagic results that this syntax applies to. */
    char *linter;
	/* The command with which to lint this type of file. */
    char *formatter;
        /* The formatting command (for programming languages mainly). */
    char *comment;
	/* The line comment prefix (and postfix) for this type of file. */
    colortype *color;
	/* The colors and their regexes used in this syntax. */
    int nmultis;
	/* How many multiline regex strings this syntax has. */
    struct syntaxtype *next;
	/* Next syntax. */
} syntaxtype;

typedef struct lintstruct {
    ssize_t lineno;
	/* Line number of the error. */
    ssize_t colno;
	/* Column # of the error. */
    char *msg;
	/* Error message text. */
    char *filename;
	/* Filename. */
    struct lintstruct *next;
	/* Next error. */
    struct lintstruct *prev;
	/* Previous error. */
} lintstruct;

/* Flags that indicate how a multiline regex applies to a line. */
#define CNONE		(1<<1)
	/* Yay, regex doesn't apply to this line at all! */
#define CBEGINBEFORE	(1<<2)
	/* Regex starts on an earlier line, ends on this one. */
#define CENDAFTER	(1<<3)
	/* Regex starts on this line and ends on a later one. */
#define CWHOLELINE	(1<<4)
	/* Whole line engulfed by the regex, start < me, end > me. */
#define CSTARTENDHERE	(1<<5)
	/* Regex starts and ends within this line. */
#define CWOULDBE	(1<<6)
	/* An unpaired start match on or before this line. */
#endif /* !DISABLE_COLOR */

/* More structure types. */
typedef struct filestruct {
    char *data;
	/* The text of this line. */
    ssize_t lineno;
	/* The number of this line. */
    struct filestruct *next;
	/* Next node. */
    struct filestruct *prev;
	/* Previous node. */
#ifndef DISABLE_COLOR
    short *multidata;
	/* Array of which multi-line regexes apply to this line. */
#endif
} filestruct;

typedef struct partition {
    filestruct *fileage;
	/* The top line of this portion of the file. */
    filestruct *top_prev;
	/* The line before the top line of this portion of the file. */
    char *top_data;
	/* The text before the beginning of the top line of this portion
	 * of the file. */
    filestruct *filebot;
	/* The bottom line of this portion of the file. */
    filestruct *bot_next;
	/* The line after the bottom line of this portion of the
	 * file. */
    char *bot_data;
	/* The text after the end of the bottom line of this portion of
	 * the file. */
} partition;

#ifndef NANO_TINY
typedef struct undo_group {
    ssize_t top_line;
	/* First line of group. */
    ssize_t bottom_line;
	/* Last line of group. */
    struct undo_group *next;
} undo_group;

typedef struct undo {
    ssize_t lineno;
    undo_type type;
	/* What type of undo this was. */
    size_t begin;
	/* Where did this action begin or end. */
    char *strdata;
	/* String type data we will use for copying the affected line back. */
    size_t wassize;
	/* The file size before the action. */
    size_t newsize;
	/* The file size after the action. */
    int xflags;
	/* Some flag data we need. */
    undo_group *grouping;
	/* Undo info specific to groups of lines. */

    /* Cut-specific stuff we need. */
    filestruct *cutbuffer;
	/* Copy of the cutbuffer. */
    filestruct *cutbottom;
	/* Copy of cutbottom. */
    ssize_t mark_begin_lineno;
	/* Mostly the line number of the current line; sometimes something else. */
    size_t mark_begin_x;
	/* The x position corresponding to the above line number. */
    struct undo *next;
	/* A pointer to the undo item of the preceding action. */
} undo;
#endif /* !NANO_TINY */

#ifndef DISABLE_HISTORIES
typedef struct poshiststruct {
    char *filename;
	/* The file. */
    ssize_t lineno;
	/* Line number we left off on. */
    ssize_t xno;
	/* x position in the file we left off on. */
    struct poshiststruct *next;
} poshiststruct;
#endif

typedef struct openfilestruct {
    char *filename;
	/* The file's name. */
    filestruct *fileage;
	/* The file's first line. */
    filestruct *filebot;
	/* The file's last line. */
    filestruct *edittop;
	/* The current top of the edit window for this file. */
    filestruct *current;
	/* The current line for this file. */
    size_t totsize;
	/* The file's total number of characters. */
    size_t firstcolumn;
	/* The starting column of the top line of the edit window.
	 * When not in softwrap mode, it's always zero. */
    size_t current_x;
	/* The file's x-coordinate position. */
    size_t placewewant;
	/* The file's x position we would like. */
    ssize_t current_y;
	/* The file's y-coordinate position. */
    bool modified;
	/* Whether the file has been modified. */
    struct stat *current_stat;
	/* The file's current stat information. */
#ifndef NANO_TINY
    bool mark_set;
	/* Whether the mark is on in this file. */
    filestruct *mark_begin;
	/* The file's line where the mark is, if any. */
    size_t mark_begin_x;
	/* The file's mark's x-coordinate position, if any. */
    mark_type kind_of_mark;
	/* Whether this is a soft or a hard mark. */
    file_format fmt;
	/* The file's format. */
    undo *undotop;
	/* The top of the undo list. */
    undo *current_undo;
	/* The current (i.e. next) level of undo. */
    undo_type last_action;
	/* The type of the last action the user performed. */
    char *lock_filename;
	/* The path of the lockfile, if we created one. */
#endif
#ifndef DISABLE_COLOR
    syntaxtype *syntax;
	/* The  syntax struct for this file, if any. */
    colortype *colorstrings;
	/* The file's associated colors. */
#endif
    struct openfilestruct *next;
	/* The next open file, if any. */
    struct openfilestruct *prev;
	/* The preceding open file, if any. */
} openfilestruct;

#ifndef DISABLE_NANORC
typedef struct rcoption {
    const char *name;
	/* The name of the rcfile option. */
    long flag;
	/* The flag associated with it, if any. */
} rcoption;
#endif

typedef struct sc {
    const char *keystr;
	/* The string that describes a keystroke, like "^C" or "M-R". */
    bool meta;
	/* Whether this is a Meta keystroke. */
    int keycode;
	/* The integer that, together with meta, identifies the keystroke. */
    int menus;
	/* Which menus this applies to. */
    void (*scfunc)(void);
	/* The function we're going to run. */
#ifndef NANO_TINY
    int toggle;
	/* If a toggle, what we're toggling. */
    int ordinal;
	/* The how-manieth toggle this is, in order to be able to
	 * keep them in sequence. */
#endif
    struct sc *next;
	/* Next in the list. */
} sc;

typedef struct subnfunc {
    void (*scfunc)(void);
	/* The actual function to call. */
    int menus;
	/* In what menus this function applies. */
    const char *desc;
	/* The function's short description, for example "Where Is". */
#ifndef DISABLE_HELP
    const char *help;
	/* The help-screen text for this function. */
    bool blank_after;
	/* Whether there should be a blank line after the help text
	 * for this function. */
#endif
    bool viewok;
	/* Is this function allowed when in view mode? */
    long toggle;
	/* If this is a toggle, which toggle to affect. */
    struct subnfunc *next;
	/* Next item in the list. */
} subnfunc;

#ifdef ENABLE_WORDCOMPLETION
typedef struct completion_word {
    char *word;
    struct completion_word *next;
} completion_word;
#endif

/* The elements of the interface that can be colored differently. */
enum
{
    TITLE_BAR = 0,
    LINE_NUMBER,
    STATUS_BAR,
    KEY_COMBO,
    FUNCTION_TAG,
    NUMBER_OF_ELEMENTS
};

/* Enumeration used in the flags array.  See the definition of FLAGMASK. */
enum
{
    DONTUSE,
    CASE_SENSITIVE,
    CONST_UPDATE,
    NO_HELP,
    SUSPEND,
    NO_WRAP,
    AUTOINDENT,
    VIEW_MODE,
    USE_MOUSE,
    USE_REGEXP,
    TEMP_FILE,
    CUT_TO_END,
    BACKWARDS_SEARCH,
    MULTIBUFFER,
    SMOOTH_SCROLL,
    REBIND_DELETE,
    REBIND_KEYPAD,
    NO_CONVERT,
    BACKUP_FILE,
    INSECURE_BACKUP,
    NO_COLOR_SYNTAX,
    PRESERVE,
    HISTORYLOG,
    RESTRICTED,
    SMART_HOME,
    WHITESPACE_DISPLAY,
    MORE_SPACE,
    TABS_TO_SPACES,
    QUICK_BLANK,
    WORD_BOUNDS,
    NO_NEWLINES,
    BOLD_TEXT,
    QUIET,
    SOFTWRAP,
    POS_HISTORY,
    LOCKING,
    NOREAD_MODE,
    MAKE_IT_UNIX,
    JUSTIFY_TRIM,
    SHOW_CURSOR,
    LINE_NUMBERS,
    NO_PAUSES
};

/* Flags for the menus in which a given function should be present. */
#define MMAIN			(1<<0)
#define MWHEREIS		(1<<1)
#define MREPLACE		(1<<2)
#define MREPLACEWITH		(1<<3)
#define MGOTOLINE		(1<<4)
#define MWRITEFILE		(1<<5)
#define MINSERTFILE		(1<<6)
#define MEXTCMD			(1<<7)
#define MHELP			(1<<8)
#define MSPELL			(1<<9)
#define MBROWSER		(1<<10)
#define MWHEREISFILE		(1<<11)
#define MGOTODIR		(1<<12)
#define MYESNO			(1<<13)
#define MLINTER			(1<<14)
/* This is an abbreviation for all menus except Help and YesNo. */
#define MMOST  (MMAIN|MWHEREIS|MREPLACE|MREPLACEWITH|MGOTOLINE|MWRITEFILE|MINSERTFILE|\
		MEXTCMD|MBROWSER|MWHEREISFILE|MGOTODIR|MSPELL|MLINTER)

/* Basic control codes. */
#define TAB_CODE  0x09
#define ESC_CODE  0x1B
#define DEL_CODE  0x7F

/* Codes for "modified" Arrow keys, beyond KEY_MAX of ncurses. */
#define CONTROL_LEFT 0x401
#define CONTROL_RIGHT 0x402
#define CONTROL_UP 0x403
#define CONTROL_DOWN 0x404
#define SHIFT_CONTROL_LEFT 0x405
#define SHIFT_CONTROL_RIGHT 0x406
#define SHIFT_CONTROL_UP 0x407
#define SHIFT_CONTROL_DOWN 0x408
#define SHIFT_ALT_LEFT 0x409
#define SHIFT_ALT_RIGHT 0x40a
#define SHIFT_ALT_UP 0x40b
#define SHIFT_ALT_DOWN 0x40c
#define SHIFT_PAGEUP 0x40d
#define SHIFT_PAGEDOWN 0x40e
#define SHIFT_HOME 0x40f
#define SHIFT_END 0x410

#ifndef NANO_TINY
/* An imaginary key for when we get a SIGWINCH (window resize). */
#define KEY_WINCH -2

/* Some extra flags for the undo function. */
#define WAS_FINAL_BACKSPACE	(1<<1)
#define WAS_WHOLE_LINE		(1<<2)
/* The flags for the mark need to be the highest. */
#define MARK_WAS_SET		(1<<3)
#define WAS_MARKED_FORWARD	(1<<4)
#endif /* !NANO_TINY */

/* The maximum number of entries displayed in the main shortcut list. */
#define MAIN_VISIBLE (((COLS + 40) / 20) * 2)

/* The default number of characters from the end of the line where
 * wrapping occurs. */
#define CHARS_FROM_EOL 8

/* The default width of a tab in spaces. */
#define WIDTH_OF_TAB 8

/* The maximum number of search/replace history strings saved, not
 * counting the blank lines at their ends. */
#define MAX_SEARCH_HISTORY 100

/* The maximum number of bytes buffered at one time. */
#define MAX_BUF_SIZE 128

/* The largest size_t number that doesn't have the high bit set. */
#define HIGHEST_POSITIVE ((~(size_t)0) >> 1)

#endif /* !NANO_H */
