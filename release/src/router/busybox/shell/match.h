/* match.h - interface to shell ##/%% matching code */

#ifndef SHELL_MATCH_H
#define SHELL_MATCH_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

//TODO! Why ash.c still uses internal version?!

typedef char *(*scan_t)(char *string, char *match, bool match_at_left);

char *scanleft(char *string, char *match, bool match_at_left);
char *scanright(char *string, char *match, bool match_at_left);

static inline scan_t pick_scan(char op1, char op2, bool *match_at_left)
{
	/* #  - scanleft
	 * ## - scanright
	 * %  - scanright
	 * %% - scanleft
	 */
	if (op1 == '#') {
		*match_at_left = true;
		return op1 == op2 ? scanright : scanleft;
	} else {
		*match_at_left = false;
		return op1 == op2 ? scanleft : scanright;
	}
}

POP_SAVED_FUNCTION_VISIBILITY

#endif
