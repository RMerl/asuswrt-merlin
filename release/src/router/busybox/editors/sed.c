/* vi: set sw=4 ts=4: */
/*
 * sed.c - very minimalist version of sed
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc. and Mark Whitley
 * Copyright (C) 1999,2000,2001 by Mark Whitley <markw@codepoet.org>
 * Copyright (C) 2002  Matt Kraai
 * Copyright (C) 2003 by Glenn McGrath
 * Copyright (C) 2003,2004 by Rob Landley <rob@landley.net>
 *
 * MAINTAINER: Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* Code overview.
 *
 * Files are laid out to avoid unnecessary function declarations.  So for
 * example, every function add_cmd calls occurs before add_cmd in this file.
 *
 * add_cmd() is called on each line of sed command text (from a file or from
 * the command line).  It calls get_address() and parse_cmd_args().  The
 * resulting sed_cmd_t structures are appended to a linked list
 * (G.sed_cmd_head/G.sed_cmd_tail).
 *
 * add_input_file() adds a FILE* to the list of input files.  We need to
 * know all input sources ahead of time to find the last line for the $ match.
 *
 * process_files() does actual sedding, reading data lines from each input FILE*
 * (which could be stdin) and applying the sed command list (sed_cmd_head) to
 * each of the resulting lines.
 *
 * sed_main() is where external code calls into this, with a command line.
 */

/* Supported features and commands in this version of sed:
 *
 * - comments ('#')
 * - address matching: num|/matchstr/[,num|/matchstr/|$]command
 * - commands: (p)rint, (d)elete, (s)ubstitue (with g & I flags)
 * - edit commands: (a)ppend, (i)nsert, (c)hange
 * - file commands: (r)ead
 * - backreferences in substitution expressions (\0, \1, \2...\9)
 * - grouped commands: {cmd1;cmd2}
 * - transliteration (y/source-chars/dest-chars/)
 * - pattern space hold space storing / swapping (g, h, x)
 * - labels / branching (: label, b, t, T)
 *
 * (Note: Specifying an address (range) to match is *optional*; commands
 * default to the whole pattern space if no specific address match was
 * requested.)
 *
 * Todo:
 * - Create a wrapper around regex to make libc's regex conform with sed
 *
 * Reference http://www.opengroup.org/onlinepubs/007904975/utilities/sed.html
 */

//usage:#define sed_trivial_usage
//usage:       "[-inr] [-f FILE]... [-e CMD]... [FILE]...\n"
//usage:       "or: sed [-inr] CMD [FILE]..."
//usage:#define sed_full_usage "\n\n"
//usage:       "	-e CMD	Add CMD to sed commands to be executed"
//usage:     "\n	-f FILE	Add FILE contents to sed commands to be executed"
//usage:     "\n	-i	Edit files in-place (else sends result to stdout)"
//usage:     "\n	-n	Suppress automatic printing of pattern space"
//usage:     "\n	-r	Use extended regex syntax"
//usage:     "\n"
//usage:     "\nIf no -e or -f, the first non-option argument is the sed command string."
//usage:     "\nRemaining arguments are input files (stdin if none)."
//usage:
//usage:#define sed_example_usage
//usage:       "$ echo \"foo\" | sed -e 's/f[a-zA-Z]o/bar/g'\n"
//usage:       "bar\n"

#include "libbb.h"
#include "xregex.h"

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif


enum {
	OPT_in_place = 1 << 0,
};

/* Each sed command turns into one of these structures. */
typedef struct sed_cmd_s {
	/* Ordered by alignment requirements: currently 36 bytes on x86 */
	struct sed_cmd_s *next; /* Next command (linked list, NULL terminated) */

	/* address storage */
	regex_t *beg_match;     /* sed -e '/match/cmd' */
	regex_t *end_match;     /* sed -e '/match/,/end_match/cmd' */
	regex_t *sub_match;     /* For 's/sub_match/string/' */
	int beg_line;           /* 'sed 1p'   0 == apply commands to all lines */
	int beg_line_orig;      /* copy of the above, needed for -i */
	int end_line;           /* 'sed 1,3p' 0 == one line only. -1 = last line ($) */

	FILE *sw_file;          /* File (sw) command writes to, -1 for none. */
	char *string;           /* Data string for (saicytb) commands. */

	unsigned which_match;   /* (s) Which match to replace (0 for all) */

	/* Bitfields (gcc won't group them if we don't) */
	unsigned invert:1;      /* the '!' after the address */
	unsigned in_match:1;    /* Next line also included in match? */
	unsigned sub_p:1;       /* (s) print option */

	char sw_last_char;      /* Last line written by (sw) had no '\n' */

	/* GENERAL FIELDS */
	char cmd;               /* The command char: abcdDgGhHilnNpPqrstwxy:={} */
} sed_cmd_t;

static const char semicolon_whitespace[] ALIGN1 = "; \n\r\t\v";

struct globals {
	/* options */
	int be_quiet, regex_type;
	FILE *nonstdout;
	char *outname, *hold_space;

	/* List of input files */
	int input_file_count, current_input_file;
	FILE **input_file_list;

	regmatch_t regmatch[10];
	regex_t *previous_regex_ptr;

	/* linked list of sed commands */
	sed_cmd_t *sed_cmd_head, **sed_cmd_tail;

	/* Linked list of append lines */
	llist_t *append_head;

	char *add_cmd_line;

	struct pipeline {
		char *buf;  /* Space to hold string */
		int idx;    /* Space used */
		int len;    /* Space allocated */
	} pipeline;
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
struct BUG_G_too_big {
	char BUG_G_too_big[sizeof(G) <= COMMON_BUFSIZE ? 1 : -1];
};
#define INIT_G() do { \
	G.sed_cmd_tail = &G.sed_cmd_head; \
} while (0)


#if ENABLE_FEATURE_CLEAN_UP
static void sed_free_and_close_stuff(void)
{
	sed_cmd_t *sed_cmd = G.sed_cmd_head;

	llist_free(G.append_head, free);

	while (sed_cmd) {
		sed_cmd_t *sed_cmd_next = sed_cmd->next;

		if (sed_cmd->sw_file)
			xprint_and_close_file(sed_cmd->sw_file);

		if (sed_cmd->beg_match) {
			regfree(sed_cmd->beg_match);
			free(sed_cmd->beg_match);
		}
		if (sed_cmd->end_match) {
			regfree(sed_cmd->end_match);
			free(sed_cmd->end_match);
		}
		if (sed_cmd->sub_match) {
			regfree(sed_cmd->sub_match);
			free(sed_cmd->sub_match);
		}
		free(sed_cmd->string);
		free(sed_cmd);
		sed_cmd = sed_cmd_next;
	}

	free(G.hold_space);

	while (G.current_input_file < G.input_file_count)
		fclose(G.input_file_list[G.current_input_file++]);
}
#else
void sed_free_and_close_stuff(void);
#endif

/* If something bad happens during -i operation, delete temp file */

static void cleanup_outname(void)
{
	if (G.outname) unlink(G.outname);
}

/* strcpy, replacing "\from" with 'to'. If to is NUL, replacing "\any" with 'any' */

static void parse_escapes(char *dest, const char *string, int len, char from, char to)
{
	int i = 0;

	while (i < len) {
		if (string[i] == '\\') {
			if (!to || string[i+1] == from) {
				*dest++ = to ? to : string[i+1];
				i += 2;
				continue;
			}
			*dest++ = string[i++];
		}
		/* TODO: is it safe wrt a string with trailing '\\' ? */
		*dest++ = string[i++];
	}
	*dest = '\0';
}

static char *copy_parsing_escapes(const char *string, int len)
{
	const char *s;
	char *dest = xmalloc(len + 1);

	/* sed recognizes \n */
	/* GNU sed also recognizes \t and \r */
	for (s = "\nn\tt\rr"; *s; s += 2) {
		parse_escapes(dest, string, len, s[1], s[0]);
		string = dest;
		len = strlen(dest);
	}
	return dest;
}


/*
 * index_of_next_unescaped_regexp_delim - walks left to right through a string
 * beginning at a specified index and returns the index of the next regular
 * expression delimiter (typically a forward slash ('/')) not preceded by
 * a backslash ('\').  A negative delimiter disables square bracket checking.
 */
static int index_of_next_unescaped_regexp_delim(int delimiter, const char *str)
{
	int bracket = -1;
	int escaped = 0;
	int idx = 0;
	char ch;

	if (delimiter < 0) {
		bracket--;
		delimiter = -delimiter;
	}

	for (; (ch = str[idx]) != '\0'; idx++) {
		if (bracket >= 0) {
			if (ch == ']'
			 && !(bracket == idx - 1 || (bracket == idx - 2 && str[idx - 1] == '^'))
			) {
				bracket = -1;
			}
		} else if (escaped)
			escaped = 0;
		else if (ch == '\\')
			escaped = 1;
		else if (bracket == -1 && ch == '[')
			bracket = idx;
		else if (ch == delimiter)
			return idx;
	}

	/* if we make it to here, we've hit the end of the string */
	bb_error_msg_and_die("unmatched '%c'", delimiter);
}

/*
 *  Returns the index of the third delimiter
 */
static int parse_regex_delim(const char *cmdstr, char **match, char **replace)
{
	const char *cmdstr_ptr = cmdstr;
	unsigned char delimiter;
	int idx = 0;

	/* verify that the 's' or 'y' is followed by something.  That something
	 * (typically a 'slash') is now our regexp delimiter... */
	if (*cmdstr == '\0')
		bb_error_msg_and_die("bad format in substitution expression");
	delimiter = *cmdstr_ptr++;

	/* save the match string */
	idx = index_of_next_unescaped_regexp_delim(delimiter, cmdstr_ptr);
	*match = copy_parsing_escapes(cmdstr_ptr, idx);

	/* save the replacement string */
	cmdstr_ptr += idx + 1;
	idx = index_of_next_unescaped_regexp_delim(- (int)delimiter, cmdstr_ptr);
	*replace = copy_parsing_escapes(cmdstr_ptr, idx);

	return ((cmdstr_ptr - cmdstr) + idx);
}

/*
 * returns the index in the string just past where the address ends.
 */
static int get_address(const char *my_str, int *linenum, regex_t ** regex)
{
	const char *pos = my_str;

	if (isdigit(*my_str)) {
		*linenum = strtol(my_str, (char**)&pos, 10);
		/* endstr shouldnt ever equal NULL */
	} else if (*my_str == '$') {
		*linenum = -1;
		pos++;
	} else if (*my_str == '/' || *my_str == '\\') {
		int next;
		char delimiter;
		char *temp;

		delimiter = '/';
		if (*my_str == '\\')
			delimiter = *++pos;
		next = index_of_next_unescaped_regexp_delim(delimiter, ++pos);
		temp = copy_parsing_escapes(pos, next);
		*regex = xzalloc(sizeof(regex_t));
		xregcomp(*regex, temp, G.regex_type|REG_NEWLINE);
		free(temp);
		/* Move position to next character after last delimiter */
		pos += (next+1);
	}
	return pos - my_str;
}

/* Grab a filename.  Whitespace at start is skipped, then goes to EOL. */
static int parse_file_cmd(/*sed_cmd_t *sed_cmd,*/ const char *filecmdstr, char **retval)
{
	int start = 0, idx, hack = 0;

	/* Skip whitespace, then grab filename to end of line */
	while (isspace(filecmdstr[start]))
		start++;
	idx = start;
	while (filecmdstr[idx] && filecmdstr[idx] != '\n')
		idx++;

	/* If lines glued together, put backslash back. */
	if (filecmdstr[idx] == '\n')
		hack = 1;
	if (idx == start)
		bb_error_msg_and_die("empty filename");
	*retval = xstrndup(filecmdstr+start, idx-start+hack+1);
	if (hack)
		(*retval)[idx] = '\\';

	return idx;
}

static int parse_subst_cmd(sed_cmd_t *sed_cmd, const char *substr)
{
	int cflags = G.regex_type;
	char *match;
	int idx;

	/*
	 * A substitution command should look something like this:
	 *    s/match/replace/ #gIpw
	 *    ||     |        |||
	 *    mandatory       optional
	 */
	idx = parse_regex_delim(substr, &match, &sed_cmd->string);

	/* determine the number of back references in the match string */
	/* Note: we compute this here rather than in the do_subst_command()
	 * function to save processor time, at the expense of a little more memory
	 * (4 bits) per sed_cmd */

	/* process the flags */

	sed_cmd->which_match = 1;
	while (substr[++idx]) {
		/* Parse match number */
		if (isdigit(substr[idx])) {
			if (match[0] != '^') {
				/* Match 0 treated as all, multiple matches we take the last one. */
				const char *pos = substr + idx;
/* FIXME: error check? */
				sed_cmd->which_match = (unsigned)strtol(substr+idx, (char**) &pos, 10);
				idx = pos - substr;
			}
			continue;
		}
		/* Skip spaces */
		if (isspace(substr[idx]))
			continue;

		switch (substr[idx]) {
		/* Replace all occurrences */
		case 'g':
			if (match[0] != '^')
				sed_cmd->which_match = 0;
			break;
		/* Print pattern space */
		case 'p':
			sed_cmd->sub_p = 1;
			break;
		/* Write to file */
		case 'w':
		{
			char *temp;
			idx += parse_file_cmd(/*sed_cmd,*/ substr+idx, &temp);
			break;
		}
		/* Ignore case (gnu exension) */
		case 'I':
			cflags |= REG_ICASE;
			break;
		/* Comment */
		case '#':
			// while (substr[++idx]) continue;
			idx += strlen(substr + idx); // same
			/* Fall through */
		/* End of command */
		case ';':
		case '}':
			goto out;
		default:
			bb_error_msg_and_die("bad option in substitution expression");
		}
	}
 out:
	/* compile the match string into a regex */
	if (*match != '\0') {
		/* If match is empty, we use last regex used at runtime */
		sed_cmd->sub_match = xzalloc(sizeof(regex_t));
		dbg("xregcomp('%s',%x)", match, cflags);
		xregcomp(sed_cmd->sub_match, match, cflags);
		dbg("regcomp ok");
	}
	free(match);

	return idx;
}

/*
 *  Process the commands arguments
 */
static const char *parse_cmd_args(sed_cmd_t *sed_cmd, const char *cmdstr)
{
	static const char cmd_letters[] = "saicrw:btTydDgGhHlnNpPqx={}";
	enum {
		IDX_s = 0,
		IDX_a,
		IDX_i,
		IDX_c,
		IDX_r,
		IDX_w,
		IDX_colon,
		IDX_b,
		IDX_t,
		IDX_T,
		IDX_y,
		IDX_d,
		IDX_D,
		IDX_g,
		IDX_G,
		IDX_h,
		IDX_H,
		IDX_l,
		IDX_n,
		IDX_N,
		IDX_p,
		IDX_P,
		IDX_q,
		IDX_x,
		IDX_equal,
		IDX_lbrace,
		IDX_rbrace,
		IDX_nul
	};
	struct chk { char chk[sizeof(cmd_letters)-1 == IDX_nul ? 1 : -1]; };

	unsigned idx = strchrnul(cmd_letters, sed_cmd->cmd) - cmd_letters;

	/* handle (s)ubstitution command */
	if (idx == IDX_s) {
		cmdstr += parse_subst_cmd(sed_cmd, cmdstr);
	}
	/* handle edit cmds: (a)ppend, (i)nsert, and (c)hange */
	else if (idx <= IDX_c) { /* a,i,c */
		if ((sed_cmd->end_line || sed_cmd->end_match) && sed_cmd->cmd != 'c')
			bb_error_msg_and_die("only a beginning address can be specified for edit commands");
		for (;;) {
			if (*cmdstr == '\n' || *cmdstr == '\\') {
				cmdstr++;
				break;
			}
			if (!isspace(*cmdstr))
				break;
			cmdstr++;
		}
		sed_cmd->string = xstrdup(cmdstr);
		/* "\anychar" -> "anychar" */
		parse_escapes(sed_cmd->string, sed_cmd->string, strlen(cmdstr), '\0', '\0');
		cmdstr += strlen(cmdstr);
	}
	/* handle file cmds: (r)ead */
	else if (idx <= IDX_w) { /* r,w */
		if (sed_cmd->end_line || sed_cmd->end_match)
			bb_error_msg_and_die("command only uses one address");
		cmdstr += parse_file_cmd(/*sed_cmd,*/ cmdstr, &sed_cmd->string);
		if (sed_cmd->cmd == 'w') {
			sed_cmd->sw_file = xfopen_for_write(sed_cmd->string);
			sed_cmd->sw_last_char = '\n';
		}
	}
	/* handle branch commands */
	else if (idx <= IDX_T) { /* :,b,t,T */
		int length;

		cmdstr = skip_whitespace(cmdstr);
		length = strcspn(cmdstr, semicolon_whitespace);
		if (length) {
			sed_cmd->string = xstrndup(cmdstr, length);
			cmdstr += length;
		}
	}
	/* translation command */
	else if (idx == IDX_y) {
		char *match, *replace;
		int i = cmdstr[0];

		cmdstr += parse_regex_delim(cmdstr, &match, &replace)+1;
		/* \n already parsed, but \delimiter needs unescaping. */
		parse_escapes(match, match, strlen(match), i, i);
		parse_escapes(replace, replace, strlen(replace), i, i);

		sed_cmd->string = xzalloc((strlen(match) + 1) * 2);
		for (i = 0; match[i] && replace[i]; i++) {
			sed_cmd->string[i*2] = match[i];
			sed_cmd->string[i*2+1] = replace[i];
		}
		free(match);
		free(replace);
	}
	/* if it wasnt a single-letter command that takes no arguments
	 * then it must be an invalid command.
	 */
	else if (idx >= IDX_nul) { /* not d,D,g,G,h,H,l,n,N,p,P,q,x,=,{,} */
		bb_error_msg_and_die("unsupported command %c", sed_cmd->cmd);
	}

	/* give back whatever's left over */
	return cmdstr;
}


/* Parse address+command sets, skipping comment lines. */

static void add_cmd(const char *cmdstr)
{
	sed_cmd_t *sed_cmd;
	unsigned len, n;

	/* Append this line to any unfinished line from last time. */
	if (G.add_cmd_line) {
		char *tp = xasprintf("%s\n%s", G.add_cmd_line, cmdstr);
		free(G.add_cmd_line);
		cmdstr = G.add_cmd_line = tp;
	}

	/* If this line ends with unescaped backslash, request next line. */
	n = len = strlen(cmdstr);
	while (n && cmdstr[n-1] == '\\')
		n--;
	if ((len - n) & 1) { /* if odd number of trailing backslashes */
		if (!G.add_cmd_line)
			G.add_cmd_line = xstrdup(cmdstr);
		G.add_cmd_line[len-1] = '\0';
		return;
	}

	/* Loop parsing all commands in this line. */
	while (*cmdstr) {
		/* Skip leading whitespace and semicolons */
		cmdstr += strspn(cmdstr, semicolon_whitespace);

		/* If no more commands, exit. */
		if (!*cmdstr) break;

		/* if this is a comment, jump past it and keep going */
		if (*cmdstr == '#') {
			/* "#n" is the same as using -n on the command line */
			if (cmdstr[1] == 'n')
				G.be_quiet++;
			cmdstr = strpbrk(cmdstr, "\n\r");
			if (!cmdstr) break;
			continue;
		}

		/* parse the command
		 * format is: [addr][,addr][!]cmd
		 *            |----||-----||-|
		 *            part1 part2  part3
		 */

		sed_cmd = xzalloc(sizeof(sed_cmd_t));

		/* first part (if present) is an address: either a '$', a number or a /regex/ */
		cmdstr += get_address(cmdstr, &sed_cmd->beg_line, &sed_cmd->beg_match);
		sed_cmd->beg_line_orig = sed_cmd->beg_line;

		/* second part (if present) will begin with a comma */
		if (*cmdstr == ',') {
			int idx;

			cmdstr++;
			idx = get_address(cmdstr, &sed_cmd->end_line, &sed_cmd->end_match);
			if (!idx)
				bb_error_msg_and_die("no address after comma");
			cmdstr += idx;
		}

		/* skip whitespace before the command */
		cmdstr = skip_whitespace(cmdstr);

		/* Check for inversion flag */
		if (*cmdstr == '!') {
			sed_cmd->invert = 1;
			cmdstr++;

			/* skip whitespace before the command */
			cmdstr = skip_whitespace(cmdstr);
		}

		/* last part (mandatory) will be a command */
		if (!*cmdstr)
			bb_error_msg_and_die("missing command");
		sed_cmd->cmd = *cmdstr++;
		cmdstr = parse_cmd_args(sed_cmd, cmdstr);

		/* Add the command to the command array */
		*G.sed_cmd_tail = sed_cmd;
		G.sed_cmd_tail = &sed_cmd->next;
	}

	/* If we glued multiple lines together, free the memory. */
	free(G.add_cmd_line);
	G.add_cmd_line = NULL;
}

/* Append to a string, reallocating memory as necessary. */

#define PIPE_GROW 64

static void pipe_putc(char c)
{
	if (G.pipeline.idx == G.pipeline.len) {
		G.pipeline.buf = xrealloc(G.pipeline.buf,
				G.pipeline.len + PIPE_GROW);
		G.pipeline.len += PIPE_GROW;
	}
	G.pipeline.buf[G.pipeline.idx++] = c;
}

static void do_subst_w_backrefs(char *line, char *replace)
{
	int i, j;

	/* go through the replacement string */
	for (i = 0; replace[i]; i++) {
		/* if we find a backreference (\1, \2, etc.) print the backref'ed * text */
		if (replace[i] == '\\') {
			unsigned backref = replace[++i] - '0';
			if (backref <= 9) {
				/* print out the text held in G.regmatch[backref] */
				if (G.regmatch[backref].rm_so != -1) {
					j = G.regmatch[backref].rm_so;
					while (j < G.regmatch[backref].rm_eo)
						pipe_putc(line[j++]);
				}
				continue;
			}
			/* I _think_ it is impossible to get '\' to be
			 * the last char in replace string. Thus we dont check
			 * for replace[i] == NUL. (counterexample anyone?) */
			/* if we find a backslash escaped character, print the character */
			pipe_putc(replace[i]);
			continue;
		}
		/* if we find an unescaped '&' print out the whole matched text. */
		if (replace[i] == '&') {
			j = G.regmatch[0].rm_so;
			while (j < G.regmatch[0].rm_eo)
				pipe_putc(line[j++]);
			continue;
		}
		/* Otherwise just output the character. */
		pipe_putc(replace[i]);
	}
}

static int do_subst_command(sed_cmd_t *sed_cmd, char **line_p)
{
	char *line = *line_p;
	int altered = 0;
	unsigned match_count = 0;
	regex_t *current_regex;

	current_regex = sed_cmd->sub_match;
	/* Handle empty regex. */
	if (!current_regex) {
		current_regex = G.previous_regex_ptr;
		if (!current_regex)
			bb_error_msg_and_die("no previous regexp");
	}
	G.previous_regex_ptr = current_regex;

	/* Find the first match */
	dbg("matching '%s'", line);
	if (REG_NOMATCH == regexec(current_regex, line, 10, G.regmatch, 0)) {
		dbg("no match");
		return 0;
	}
	dbg("match");

	/* Initialize temporary output buffer. */
	G.pipeline.buf = xmalloc(PIPE_GROW);
	G.pipeline.len = PIPE_GROW;
	G.pipeline.idx = 0;

	/* Now loop through, substituting for matches */
	do {
		int i;

		/* Work around bug in glibc regexec, demonstrated by:
		 * echo " a.b" | busybox sed 's [^ .]* x g'
		 * The match_count check is so not to break
		 * echo "hi" | busybox sed 's/^/!/g'
		 */
		if (!G.regmatch[0].rm_so && !G.regmatch[0].rm_eo && match_count) {
			pipe_putc(*line++);
			goto next;
		}

		match_count++;

		/* If we aren't interested in this match, output old line to
		   end of match and continue */
		if (sed_cmd->which_match
		 && (sed_cmd->which_match != match_count)
		) {
			for (i = 0; i < G.regmatch[0].rm_eo; i++)
				pipe_putc(*line++);
			goto next;
		}

		/* print everything before the match */
		for (i = 0; i < G.regmatch[0].rm_so; i++)
			pipe_putc(line[i]);

		/* then print the substitution string */
		do_subst_w_backrefs(line, sed_cmd->string);

		/* advance past the match */
		line += G.regmatch[0].rm_eo;
		/* flag that something has changed */
		altered++;

		/* if we're not doing this globally, get out now */
		if (sed_cmd->which_match != 0)
			break;
 next:
		if (*line == '\0')
			break;

//maybe (G.regmatch[0].rm_eo ? REG_NOTBOL : 0) instead of unconditional REG_NOTBOL?
	} while (regexec(current_regex, line, 10, G.regmatch, REG_NOTBOL) != REG_NOMATCH);

	/* Copy rest of string into output pipeline */
	while (1) {
		char c = *line++;
		pipe_putc(c);
		if (c == '\0')
			break;
	}

	free(*line_p);
	*line_p = G.pipeline.buf;
	return altered;
}

/* Set command pointer to point to this label.  (Does not handle null label.) */
static sed_cmd_t *branch_to(char *label)
{
	sed_cmd_t *sed_cmd;

	for (sed_cmd = G.sed_cmd_head; sed_cmd; sed_cmd = sed_cmd->next) {
		if (sed_cmd->cmd == ':' && sed_cmd->string && !strcmp(sed_cmd->string, label)) {
			return sed_cmd;
		}
	}
	bb_error_msg_and_die("can't find label for jump to '%s'", label);
}

static void append(char *s)
{
	llist_add_to_end(&G.append_head, xstrdup(s));
}

static void flush_append(void)
{
	char *data;

	/* Output appended lines. */
	while ((data = (char *)llist_pop(&G.append_head))) {
		fprintf(G.nonstdout, "%s\n", data);
		free(data);
	}
}

static void add_input_file(FILE *file)
{
	G.input_file_list = xrealloc_vector(G.input_file_list, 2, G.input_file_count);
	G.input_file_list[G.input_file_count++] = file;
}

/* Get next line of input from G.input_file_list, flushing append buffer and
 * noting if we ran out of files without a newline on the last line we read.
 */
enum {
	NO_EOL_CHAR = 1,
	LAST_IS_NUL = 2,
};
static char *get_next_line(char *gets_char)
{
	char *temp = NULL;
	int len;
	char gc;

	flush_append();

	/* will be returned if last line in the file
	 * doesn't end with either '\n' or '\0' */
	gc = NO_EOL_CHAR;
	while (G.current_input_file < G.input_file_count) {
		FILE *fp = G.input_file_list[G.current_input_file];
		/* Read line up to a newline or NUL byte, inclusive,
		 * return malloc'ed char[]. length of the chunk read
		 * is stored in len. NULL if EOF/error */
		temp = bb_get_chunk_from_file(fp, &len);
		if (temp) {
			/* len > 0 here, it's ok to do temp[len-1] */
			char c = temp[len-1];
			if (c == '\n' || c == '\0') {
				temp[len-1] = '\0';
				gc = c;
				if (c == '\0') {
					int ch = fgetc(fp);
					if (ch != EOF)
						ungetc(ch, fp);
					else
						gc = LAST_IS_NUL;
				}
			}
			/* else we put NO_EOL_CHAR into *gets_char */
			break;

		/* NB: I had the idea of peeking next file(s) and returning
		 * NO_EOL_CHAR only if it is the *last* non-empty
		 * input file. But there is a case where this won't work:
		 * file1: "a woo\nb woo"
		 * file2: "c no\nd no"
		 * sed -ne 's/woo/bang/p' input1 input2 => "a bang\nb bang"
		 * (note: *no* newline after "b bang"!) */
		}
		/* Close this file and advance to next one */
		fclose(fp);
		G.current_input_file++;
	}
	*gets_char = gc;
	return temp;
}

/* Output line of text. */
/* Note:
 * The tricks with NO_EOL_CHAR and last_puts_char are there to emulate gnu sed.
 * Without them, we had this:
 * echo -n thingy >z1
 * echo -n again >z2
 * >znull
 * sed "s/i/z/" z1 z2 znull | hexdump -vC
 * output:
 * gnu sed 4.1.5:
 * 00000000  74 68 7a 6e 67 79 0a 61  67 61 7a 6e              |thzngy.agazn|
 * bbox:
 * 00000000  74 68 7a 6e 67 79 61 67  61 7a 6e                 |thzngyagazn|
 */
static void puts_maybe_newline(char *s, FILE *file, char *last_puts_char, char last_gets_char)
{
	char lpc = *last_puts_char;

	/* Need to insert a '\n' between two files because first file's
	 * last line wasn't terminated? */
	if (lpc != '\n' && lpc != '\0') {
		fputc('\n', file);
		lpc = '\n';
	}
	fputs(s, file);

	/* 'x' - just something which is not '\n', '\0' or NO_EOL_CHAR */
	if (s[0])
		lpc = 'x';

	/* had trailing '\0' and it was last char of file? */
	if (last_gets_char == LAST_IS_NUL) {
		fputc('\0', file);
		lpc = 'x'; /* */
	} else
	/* had trailing '\n' or '\0'? */
	if (last_gets_char != NO_EOL_CHAR) {
		fputc(last_gets_char, file);
		lpc = last_gets_char;
	}

	if (ferror(file)) {
		xfunc_error_retval = 4;  /* It's what gnu sed exits with... */
		bb_error_msg_and_die(bb_msg_write_error);
	}
	*last_puts_char = lpc;
}

#define sed_puts(s, n) (puts_maybe_newline(s, G.nonstdout, &last_puts_char, n))

static int beg_match(sed_cmd_t *sed_cmd, const char *pattern_space)
{
	int retval = sed_cmd->beg_match && !regexec(sed_cmd->beg_match, pattern_space, 0, NULL, 0);
	if (retval)
		G.previous_regex_ptr = sed_cmd->beg_match;
	return retval;
}

/* Process all the lines in all the files */

static void process_files(void)
{
	char *pattern_space, *next_line;
	int linenum = 0;
	char last_puts_char = '\n';
	char last_gets_char, next_gets_char;
	sed_cmd_t *sed_cmd;
	int substituted;

	/* Prime the pump */
	next_line = get_next_line(&next_gets_char);

	/* Go through every line in each file */
 again:
	substituted = 0;

	/* Advance to next line.  Stop if out of lines. */
	pattern_space = next_line;
	if (!pattern_space)
		return;
	last_gets_char = next_gets_char;

	/* Read one line in advance so we can act on the last line,
	 * the '$' address */
	next_line = get_next_line(&next_gets_char);
	linenum++;

	/* For every line, go through all the commands */
 restart:
	for (sed_cmd = G.sed_cmd_head; sed_cmd; sed_cmd = sed_cmd->next) {
		int old_matched, matched;

		old_matched = sed_cmd->in_match;

		/* Determine if this command matches this line: */

		dbg("match1:%d", sed_cmd->in_match);
		dbg("match2:%d", (!sed_cmd->beg_line && !sed_cmd->end_line
				&& !sed_cmd->beg_match && !sed_cmd->end_match));
		dbg("match3:%d", (sed_cmd->beg_line > 0
			&& (sed_cmd->end_line || sed_cmd->end_match
			    ? (sed_cmd->beg_line <= linenum)
			    : (sed_cmd->beg_line == linenum)
			    )
			));
		dbg("match4:%d", (beg_match(sed_cmd, pattern_space)));
		dbg("match5:%d", (sed_cmd->beg_line == -1 && next_line == NULL));

		/* Are we continuing a previous multi-line match? */
		sed_cmd->in_match = sed_cmd->in_match
			/* Or is no range necessary? */
			|| (!sed_cmd->beg_line && !sed_cmd->end_line
				&& !sed_cmd->beg_match && !sed_cmd->end_match)
			/* Or did we match the start of a numerical range? */
			|| (sed_cmd->beg_line > 0
			    && (sed_cmd->end_line || sed_cmd->end_match
				  /* note: even if end is numeric and is < linenum too,
				   * GNU sed matches! We match too, therefore we don't
				   * check here that linenum <= end.
				   * Example:
				   * printf '1\n2\n3\n4\n' | sed -n '1{N;N;d};1p;2,3p;3p;4p'
				   * first three input lines are deleted;
				   * 4th line is matched and printed
				   * by "2,3" (!) and by "4" ranges
				   */
				? (sed_cmd->beg_line <= linenum)    /* N,end */
				: (sed_cmd->beg_line == linenum)    /* N */
				)
			    )
			/* Or does this line match our begin address regex? */
			|| (beg_match(sed_cmd, pattern_space))
			/* Or did we match last line of input? */
			|| (sed_cmd->beg_line == -1 && next_line == NULL);

		/* Snapshot the value */
		matched = sed_cmd->in_match;

		dbg("cmd:'%c' matched:%d beg_line:%d end_line:%d linenum:%d",
			sed_cmd->cmd, matched, sed_cmd->beg_line, sed_cmd->end_line, linenum);

		/* Is this line the end of the current match? */

		if (matched) {
			/* once matched, "n,xxx" range is dead, disabling it */
			if (sed_cmd->beg_line > 0) {
				sed_cmd->beg_line = -2;
			}
			sed_cmd->in_match = !(
				/* has the ending line come, or is this a single address command? */
				(sed_cmd->end_line
					? sed_cmd->end_line == -1
						? !next_line
						: (sed_cmd->end_line <= linenum)
					: !sed_cmd->end_match
				)
				/* or does this line matches our last address regex */
				|| (sed_cmd->end_match && old_matched
				     && (regexec(sed_cmd->end_match,
				                 pattern_space, 0, NULL, 0) == 0)
				)
			);
		}

		/* Skip blocks of commands we didn't match */
		if (sed_cmd->cmd == '{') {
			if (sed_cmd->invert ? matched : !matched) {
				unsigned nest_cnt = 0;
				while (1) {
					if (sed_cmd->cmd == '{')
						nest_cnt++;
					if (sed_cmd->cmd == '}') {
						nest_cnt--;
						if (nest_cnt == 0)
							break;
					}
					sed_cmd = sed_cmd->next;
					if (!sed_cmd)
						bb_error_msg_and_die("unterminated {");
				}
			}
			continue;
		}

		/* Okay, so did this line match? */
		if (sed_cmd->invert ? matched : !matched)
			continue; /* no */

		/* Update last used regex in case a blank substitute BRE is found */
		if (sed_cmd->beg_match) {
			G.previous_regex_ptr = sed_cmd->beg_match;
		}

		/* actual sedding */
		dbg("pattern_space:'%s' next_line:'%s' cmd:%c",
				pattern_space, next_line, sed_cmd->cmd);
		switch (sed_cmd->cmd) {

		/* Print line number */
		case '=':
			fprintf(G.nonstdout, "%d\n", linenum);
			break;

		/* Write the current pattern space up to the first newline */
		case 'P':
		{
			char *tmp = strchr(pattern_space, '\n');
			if (tmp) {
				*tmp = '\0';
				/* TODO: explain why '\n' below */
				sed_puts(pattern_space, '\n');
				*tmp = '\n';
				break;
			}
			/* Fall Through */
		}

		/* Write the current pattern space to output */
		case 'p':
			/* NB: we print this _before_ the last line
			 * (of current file) is printed. Even if
			 * that line is nonterminated, we print
			 * '\n' here (gnu sed does the same) */
			sed_puts(pattern_space, '\n');
			break;
		/* Delete up through first newline */
		case 'D':
		{
			char *tmp = strchr(pattern_space, '\n');
			if (tmp) {
				overlapping_strcpy(pattern_space, tmp + 1);
				goto restart;
			}
		}
		/* discard this line. */
		case 'd':
			goto discard_line;

		/* Substitute with regex */
		case 's':
			if (!do_subst_command(sed_cmd, &pattern_space))
				break;
			dbg("do_subst_command succeeeded:'%s'", pattern_space);
			substituted |= 1;

			/* handle p option */
			if (sed_cmd->sub_p)
				sed_puts(pattern_space, last_gets_char);
			/* handle w option */
			if (sed_cmd->sw_file)
				puts_maybe_newline(
					pattern_space, sed_cmd->sw_file,
					&sed_cmd->sw_last_char, last_gets_char);
			break;

		/* Append line to linked list to be printed later */
		case 'a':
			append(sed_cmd->string);
			break;

		/* Insert text before this line */
		case 'i':
			sed_puts(sed_cmd->string, '\n');
			break;

		/* Cut and paste text (replace) */
		case 'c':
			/* Only triggers on last line of a matching range. */
			if (!sed_cmd->in_match)
				sed_puts(sed_cmd->string, '\n');
			goto discard_line;

		/* Read file, append contents to output */
		case 'r':
		{
			FILE *rfile;
			rfile = fopen_for_read(sed_cmd->string);
			if (rfile) {
				char *line;

				while ((line = xmalloc_fgetline(rfile))
						!= NULL)
					append(line);
				xprint_and_close_file(rfile);
			}

			break;
		}

		/* Write pattern space to file. */
		case 'w':
			puts_maybe_newline(
				pattern_space, sed_cmd->sw_file,
				&sed_cmd->sw_last_char, last_gets_char);
			break;

		/* Read next line from input */
		case 'n':
			if (!G.be_quiet)
				sed_puts(pattern_space, last_gets_char);
			if (next_line) {
				free(pattern_space);
				pattern_space = next_line;
				last_gets_char = next_gets_char;
				next_line = get_next_line(&next_gets_char);
				substituted = 0;
				linenum++;
				break;
			}
			/* fall through */

		/* Quit.  End of script, end of input. */
		case 'q':
			/* Exit the outer while loop */
			free(next_line);
			next_line = NULL;
			goto discard_commands;

		/* Append the next line to the current line */
		case 'N':
		{
			int len;
			/* If no next line, jump to end of script and exit. */
			/* http://www.gnu.org/software/sed/manual/sed.html:
			 * "Most versions of sed exit without printing anything
			 * when the N command is issued on the last line of
			 * a file. GNU sed prints pattern space before exiting
			 * unless of course the -n command switch has been
			 * specified. This choice is by design."
			 */
			if (next_line == NULL) {
				//goto discard_line;
				goto discard_commands; /* GNU behavior */
			}
			/* Append next_line, read new next_line. */
			len = strlen(pattern_space);
			pattern_space = xrealloc(pattern_space, len + strlen(next_line) + 2);
			pattern_space[len] = '\n';
			strcpy(pattern_space + len+1, next_line);
			last_gets_char = next_gets_char;
			next_line = get_next_line(&next_gets_char);
			linenum++;
			break;
		}

		/* Test/branch if substitution occurred */
		case 't':
			if (!substituted) break;
			substituted = 0;
			/* Fall through */
		/* Test/branch if substitution didn't occur */
		case 'T':
			if (substituted) break;
			/* Fall through */
		/* Branch to label */
		case 'b':
			if (!sed_cmd->string) goto discard_commands;
			else sed_cmd = branch_to(sed_cmd->string);
			break;
		/* Transliterate characters */
		case 'y':
		{
			int i, j;
			for (i = 0; pattern_space[i]; i++) {
				for (j = 0; sed_cmd->string[j]; j += 2) {
					if (pattern_space[i] == sed_cmd->string[j]) {
						pattern_space[i] = sed_cmd->string[j + 1];
						break;
					}
				}
			}

			break;
		}
		case 'g':	/* Replace pattern space with hold space */
			free(pattern_space);
			pattern_space = xstrdup(G.hold_space ? G.hold_space : "");
			break;
		case 'G':	/* Append newline and hold space to pattern space */
		{
			int pattern_space_size = 2;
			int hold_space_size = 0;

			if (pattern_space)
				pattern_space_size += strlen(pattern_space);
			if (G.hold_space)
				hold_space_size = strlen(G.hold_space);
			pattern_space = xrealloc(pattern_space,
					pattern_space_size + hold_space_size);
			if (pattern_space_size == 2)
				pattern_space[0] = 0;
			strcat(pattern_space, "\n");
			if (G.hold_space)
				strcat(pattern_space, G.hold_space);
			last_gets_char = '\n';

			break;
		}
		case 'h':	/* Replace hold space with pattern space */
			free(G.hold_space);
			G.hold_space = xstrdup(pattern_space);
			break;
		case 'H':	/* Append newline and pattern space to hold space */
		{
			int hold_space_size = 2;
			int pattern_space_size = 0;

			if (G.hold_space)
				hold_space_size += strlen(G.hold_space);
			if (pattern_space)
				pattern_space_size = strlen(pattern_space);
			G.hold_space = xrealloc(G.hold_space,
					hold_space_size + pattern_space_size);

			if (hold_space_size == 2)
				*G.hold_space = 0;
			strcat(G.hold_space, "\n");
			if (pattern_space)
				strcat(G.hold_space, pattern_space);

			break;
		}
		case 'x': /* Exchange hold and pattern space */
		{
			char *tmp = pattern_space;
			pattern_space = G.hold_space ? G.hold_space : xzalloc(1);
			last_gets_char = '\n';
			G.hold_space = tmp;
			break;
		}
		} /* switch */
	} /* for each cmd */

	/*
	 * Exit point from sedding...
	 */
 discard_commands:
	/* we will print the line unless we were told to be quiet ('-n')
	   or if the line was suppressed (ala 'd'elete) */
	if (!G.be_quiet)
		sed_puts(pattern_space, last_gets_char);

	/* Delete and such jump here. */
 discard_line:
	flush_append();
	free(pattern_space);

	goto again;
}

/* It is possible to have a command line argument with embedded
 * newlines.  This counts as multiple command lines.
 * However, newline can be escaped: 's/e/z\<newline>z/'
 * We check for this.
 */

static void add_cmd_block(char *cmdstr)
{
	char *sv, *eol;

	cmdstr = sv = xstrdup(cmdstr);
	do {
		eol = strchr(cmdstr, '\n');
 next:
		if (eol) {
			/* Count preceding slashes */
			int slashes = 0;
			char *sl = eol;

			while (sl != cmdstr && *--sl == '\\')
				slashes++;
			/* Odd number of preceding slashes - newline is escaped */
			if (slashes & 1) {
				overlapping_strcpy(eol - 1, eol);
				eol = strchr(eol, '\n');
				goto next;
			}
			*eol = '\0';
		}
		add_cmd(cmdstr);
		cmdstr = eol + 1;
	} while (eol);
	free(sv);
}

int sed_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sed_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
	llist_t *opt_e, *opt_f;
	int status = EXIT_SUCCESS;

	INIT_G();

	/* destroy command strings on exit */
	if (ENABLE_FEATURE_CLEAN_UP) atexit(sed_free_and_close_stuff);

	/* Lie to autoconf when it starts asking stupid questions. */
	if (argv[1] && !strcmp(argv[1], "--version")) {
		puts("This is not GNU sed version 4.0");
		return 0;
	}

	/* do normal option parsing */
	opt_e = opt_f = NULL;
	opt_complementary = "e::f::" /* can occur multiple times */
	                    "nn"; /* count -n */
	/* -i must be first, to match OPT_in_place definition */
	opt = getopt32(argv, "irne:f:", &opt_e, &opt_f,
			    &G.be_quiet); /* counter for -n */
	//argc -= optind;
	argv += optind;
	if (opt & OPT_in_place) { // -i
		atexit(cleanup_outname);
	}
	if (opt & 0x2) G.regex_type |= REG_EXTENDED; // -r
	//if (opt & 0x4) G.be_quiet++; // -n
	while (opt_e) { // -e
		add_cmd_block(llist_pop(&opt_e));
	}
	while (opt_f) { // -f
		char *line;
		FILE *cmdfile;
		cmdfile = xfopen_for_read(llist_pop(&opt_f));
		while ((line = xmalloc_fgetline(cmdfile)) != NULL) {
			add_cmd(line);
			free(line);
		}
		fclose(cmdfile);
	}
	/* if we didn't get a pattern from -e or -f, use argv[0] */
	if (!(opt & 0x18)) {
		if (!*argv)
			bb_show_usage();
		add_cmd_block(*argv++);
	}
	/* Flush any unfinished commands. */
	add_cmd("");

	/* By default, we write to stdout */
	G.nonstdout = stdout;

	/* argv[0..(argc-1)] should be names of file to process. If no
	 * files were specified or '-' was specified, take input from stdin.
	 * Otherwise, we process all the files specified. */
	if (argv[0] == NULL) {
		if (opt & OPT_in_place)
			bb_error_msg_and_die(bb_msg_requires_arg, "-i");
		add_input_file(stdin);
	} else {
		int i;

		for (i = 0; argv[i]; i++) {
			struct stat statbuf;
			int nonstdoutfd;
			FILE *file;
			sed_cmd_t *sed_cmd;

			if (LONE_DASH(argv[i]) && !(opt & OPT_in_place)) {
				add_input_file(stdin);
				process_files();
				continue;
			}
			file = fopen_or_warn(argv[i], "r");
			if (!file) {
				status = EXIT_FAILURE;
				continue;
			}
			add_input_file(file);
			if (!(opt & OPT_in_place)) {
				continue;
			}

			/* -i: process each FILE separately: */

			G.outname = xasprintf("%sXXXXXX", argv[i]);
			nonstdoutfd = xmkstemp(G.outname);
			G.nonstdout = xfdopen_for_write(nonstdoutfd);

			/* Set permissions/owner of output file */
			fstat(fileno(file), &statbuf);
			/* chmod'ing AFTER chown would preserve suid/sgid bits,
			 * but GNU sed 4.2.1 does not preserve them either */
			fchmod(nonstdoutfd, statbuf.st_mode);
			fchown(nonstdoutfd, statbuf.st_uid, statbuf.st_gid);

			process_files();
			fclose(G.nonstdout);
			G.nonstdout = stdout;

			/* unlink(argv[i]); */
			xrename(G.outname, argv[i]);
			free(G.outname);
			G.outname = NULL;

			/* Re-enable disabled range matches */
			for (sed_cmd = G.sed_cmd_head; sed_cmd; sed_cmd = sed_cmd->next) {
				sed_cmd->beg_line = sed_cmd->beg_line_orig;
			}
		}
		/* Here, to handle "sed 'cmds' nonexistent_file" case we did:
		 * if (G.current_input_file >= G.input_file_count)
		 *	return status;
		 * but it's not needed since process_files() works correctly
		 * in this case too. */
	}
	process_files();

	return status;
}
