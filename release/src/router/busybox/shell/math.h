/* math.h - interface to shell math "library" -- this allows shells to share
 *          the implementation of arithmetic $((...)) expansions.
 *
 * This aims to be a POSIX shell math library as documented here:
 *	http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_04
 *
 * See math.c for internal documentation.
 */

/* The math library has just one function:
 *
 *	arith_t arith(const char *expr, int *perrcode, arith_eval_hooks_t *hooks);
 *
 *	The first argument is the math string to parse.  All normal expansions must
 *	be done already.  i.e. no dollar symbols should be present.
 *
 *	The second argument is a semi-detailed error description in case something
 *	goes wrong in the parsing steps.  Currently, those values are (for
 *	compatibility, you should assume all negative values are errors):
 *		 0 - no errors (yay!)
 *		-1 - unspecified problem
 *		-2 - divide by zero
 *		-3 - exponent less than 0
 *		-5 - expression recursion loop detected
 *
 *	The third argument is a struct pointer of hooks for your shell (see below).
 *
 *	The function returns the answer to the expression.  So if you called it
 *	with the expression:
 *		"1 + 2 + 3"
 *	You would obviously get back 6.
 */

/* To add support to a shell, you need to implement three functions:
 *
 *	lookupvar() - look up and return the value of a variable
 *
 *		If the shell does:
 *			foo=123
 *		Then the code:
 *			const char *val = lookupvar("foo");
 *		Will result in val pointing to "123"
 *
 *	setvar() - set a variable to some value
 *
 *		If the arithmetic expansion does something like:
 *			$(( i = 1))
 *		Then the math code will make a call like so:
 *			setvar("i", "1", 0);
 *		The storage for the first two parameters are not allocated, so your
 *		shell implementation will most likely need to strdup() them to save.
 *
 *	endofname() - return the end of a variable name from input
 *
 *		The arithmetic code does not know about variable naming conventions.
 *		So when it is given an experession, it knows something is not numeric,
 *		but it is up to the shell to dictate what is a valid identifiers.
 *		So when it encounters something like:
 *			$(( some_var + 123 ))
 *		It will make a call like so:
 *			end = endofname("some_var + 123");
 *		So the shell needs to scan the input string and return a pointer to the
 *		first non-identifier string.  In this case, it should return the input
 *		pointer with an offset pointing to the first space.  The typical
 *		implementation will return the offset of first char that does not match
 *		the regex (in C locale): ^[a-zA-Z_][a-zA-Z_0-9]*
 */

/* To make your life easier when dealing with optional 64bit math support,
 * rather than assume that the type is "signed long" and you can always
 * use "%ld" to scan/print the value, use the arith_t helper defines.  See
 * below for the exact things that are available.
 */

#ifndef SHELL_MATH_H
#define SHELL_MATH_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

#if ENABLE_SH_MATH_SUPPORT_64
typedef long long arith_t;
#define arith_t_fmt "%lld"
#define strto_arith_t strtoull
#else
typedef long arith_t;
#define arith_t_fmt "%ld"
#define strto_arith_t strtoul
#endif

typedef const char* FAST_FUNC (*arith_var_lookup_t)(const char *name);
typedef void        FAST_FUNC (*arith_var_set_t)(const char *name, const char *val);
typedef char*       FAST_FUNC (*arith_var_endofname_t)(const char *name);

typedef struct arith_eval_hooks {
	arith_var_lookup_t    lookupvar;
	arith_var_set_t       setvar;
	arith_var_endofname_t endofname;
} arith_eval_hooks_t;

arith_t arith(const char *expr, int *perrcode, arith_eval_hooks_t*);

POP_SAVED_FUNCTION_VISIBILITY

#endif
