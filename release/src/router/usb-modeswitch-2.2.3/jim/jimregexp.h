#ifndef JIMREGEXP_H
#define JIMREGEXP_H

#ifndef _JIMAUTOCONF_H
#error Need jimautoconf.h
#endif

#if defined(HAVE_REGCOMP) && !defined(JIM_REGEXP)
/* Use POSIX regex */
#include <regex.h>

#else

#include <stdlib.h>

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 *
 * 11/04/02 (seiwald) - const-ing for string literals
 */

typedef struct {
	int rm_so;
	int rm_eo;
} regmatch_t;

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	char that must begin a match; '\0' if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 * regmlen	length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
 * it anyway.
 */

typedef struct regexp {
	/* -- public -- */
	int re_nsub;		/* number of parenthesized subexpressions */

	/* -- private -- */
	int cflags;			/* Flags used when compiling */
	int err;			/* Any error which occurred during compile */
	int regstart;		/* Internal use only. */
	int reganch;		/* Internal use only. */
	int regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	int *program;		/* Allocated */

	/* working state - compile */
	const char *regparse;		/* Input-scan pointer. */
	int p;				/* Current output pos in program */
	int proglen;		/* Allocated program size */

	/* working state - exec */
	int eflags;				/* Flags used when executing */
	const char *start;		/* Initial string pointer. */
	const char *reginput;	/* Current input pointer. */
	const char *regbol;		/* Beginning of input, for ^ check. */

	/* Input to regexec() */
	regmatch_t *pmatch;		/* submatches will be stored here */
	int nmatch;				/* size of pmatch[] */
} regexp;

typedef regexp regex_t;

#define REG_EXTENDED 0
#define REG_NEWLINE 1
#define REG_ICASE 2

#define REG_NOTBOL 16

enum {
	REG_NOERROR,      /* Success.  */
	REG_NOMATCH,      /* Didn't find a match (for regexec).  */
	REG_BADPAT,		  /* >= REG_BADPAT is an error */
	REG_ERR_NULL_ARGUMENT,
	REG_ERR_UNKNOWN,
	REG_ERR_TOO_BIG,
	REG_ERR_NOMEM,
	REG_ERR_TOO_MANY_PAREN,
	REG_ERR_UNMATCHED_PAREN,
	REG_ERR_UNMATCHED_BRACES,
	REG_ERR_BAD_COUNT,
	REG_ERR_JUNK_ON_END,
	REG_ERR_OPERAND_COULD_BE_EMPTY,
	REG_ERR_NESTED_COUNT,
	REG_ERR_INTERNAL,
	REG_ERR_COUNT_FOLLOWS_NOTHING,
	REG_ERR_TRAILING_BACKSLASH,
	REG_ERR_CORRUPTED,
	REG_ERR_NULL_CHAR,
	REG_ERR_NUM
};

int regcomp(regex_t *preg, const char *regex, int cflags);
int regexec(regex_t  *preg,  const  char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf,  size_t errbuf_size);
void regfree(regex_t *preg);

#endif

#endif
