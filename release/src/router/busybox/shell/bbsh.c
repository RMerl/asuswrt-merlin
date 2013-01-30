/* vi: set ts=4 :
 *
 * bbsh - busybox shell
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

// A section of code that gets repeatedly or conditionally executed is stored
// as a string and parsed each time it's run.



// Wheee, debugging.

// Terminal control
#define ENABLE_BBSH_TTY        0

// &, fg, bg, jobs.  (ctrl-z with tty.)
#define ENABLE_BBSH_JOBCTL     0

// Flow control (if, while, for, functions { })
#define ENABLE_BBSH_FLOWCTL    0

#define ENABLE_BBSH_ENVVARS    0  // Environment variable support

// Local and synthetic variables, fancy prompts, set, $?, etc.
#define ENABLE_BBSH_LOCALVARS  0

// Pipes and redirects: | > < >> << && || & () ;
#define ENABLE_BBSH_PIPES      0

/* Fun:

  echo `echo hello#comment " woot` and more
*/

#include "libbb.h"

// A single executable, its arguments, and other information we know about it.
#define BBSH_FLAG_EXIT    1
#define BBSH_FLAG_SUSPEND 2
#define BBSH_FLAG_PIPE    4
#define BBSH_FLAG_AND     8
#define BBSH_FLAG_OR      16
#define BBSH_FLAG_AMP     32
#define BBSH_FLAG_SEMI    64
#define BBSH_FLAG_PAREN   128

// What we know about a single process.
struct command {
	struct command *next;
	int flags;		// exit, suspend, && ||
	int pid;		// pid (or exit code)
	int argc;
	char *argv[];
};

// A collection of processes piped into/waiting on each other.
struct pipeline {
	struct pipeline *next;
	int job_id;
	struct command *cmd;
	char *cmdline;
	int cmdlinelen;
};

static void free_list(void *list, void (*freeit)(void *data))
{
	while (list) {
		void **next = (void **)list;
		void *list_next = *next;
		freeit(list);
		free(list);
		list = list_next;
	}
}

// Parse one word from the command line, appending one or more argv[] entries
// to struct command.  Handles environment variable substitution and
// substrings.  Returns pointer to next used byte, or NULL if it
// hit an ending token.
static char *parse_word(char *start, struct command **cmd)
{
	char *end;

	// Detect end of line (and truncate line at comment)
	if (ENABLE_BBSH_PIPES && strchr("><&|(;", *start)) return 0;

	// Grab next word.  (Add dequote and envvar logic here)
	end = start;
	end = skip_non_whitespace(end);
	(*cmd)->argv[(*cmd)->argc++] = xstrndup(start, end-start);

	// Allocate more space if there's no room for NULL terminator.

	if (!((*cmd)->argc & 7))
			*cmd = xrealloc(*cmd,
					sizeof(struct command) + ((*cmd)->argc+8)*sizeof(char *));
	(*cmd)->argv[(*cmd)->argc] = 0;
	return end;
}

// Parse a line of text into a pipeline.
// Returns a pointer to the next line.

static char *parse_pipeline(char *cmdline, struct pipeline *line)
{
	struct command **cmd = &(line->cmd);
	char *start = line->cmdline = cmdline;

	if (!cmdline) return 0;

	if (ENABLE_BBSH_JOBCTL) line->cmdline = cmdline;

	// Parse command into argv[]
	for (;;) {
		char *end;

		// Skip leading whitespace and detect end of line.
		start = skip_whitespace(start);
		if (!*start || *start=='#') {
			if (ENABLE_BBSH_JOBCTL) line->cmdlinelen = start-cmdline;
			return 0;
		}

		// Allocate next command structure if necessary
		if (!*cmd) *cmd = xzalloc(sizeof(struct command)+8*sizeof(char *));

		// Parse next argument and add the results to argv[]
		end = parse_word(start, cmd);

		// If we hit the end of this command, how did it end?
		if (!end) {
			if (ENABLE_BBSH_PIPES && *start) {
				if (*start==';') {
					start++;
					break;
				}
				// handle | & < > >> << || &&
			}
			break;
		}
		start = end;
	}

	if (ENABLE_BBSH_JOBCTL) line->cmdlinelen = start-cmdline;

	return start;
}

// Execute the commands in a pipeline
static int run_pipeline(struct pipeline *line)
{
	struct command *cmd = line->cmd;
	if (!cmd || !cmd->argc) return 0;

	// Handle local commands.  This is totally fake and plastic.
	if (cmd->argc==2 && !strcmp(cmd->argv[0],"cd"))
		chdir(cmd->argv[1]);
	else if (!strcmp(cmd->argv[0],"exit"))
		exit(cmd->argc>1 ? atoi(cmd->argv[1]) : 0);
	else {
		int status;
		pid_t pid=fork();
		if (!pid) {
			run_applet_and_exit(cmd->argv[0],cmd->argc,cmd->argv);
			execvp(cmd->argv[0],cmd->argv);
			printf("No %s", cmd->argv[0]);
			exit(EXIT_FAILURE);
		} else waitpid(pid, &status, 0);
	}

	return 0;
}

static void free_cmd(void *data)
{
	struct command *cmd=(struct command *)data;

	while (cmd->argc) free(cmd->argv[--cmd->argc]);
}


static void handle(char *command)
{
	struct pipeline line;
	char *start = command;

	for (;;) {
		memset(&line,0,sizeof(struct pipeline));
		start = parse_pipeline(start, &line);
		if (!line.cmd) break;

		run_pipeline(&line);
		free_list(line.cmd, free_cmd);
	}
}

int bbsh_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bbsh_main(int argc UNUSED_PARAM, char **argv)
{
	char *command=NULL;
	FILE *f;

	getopt32(argv, "c:", &command);

	f = argv[optind] ? xfopen_for_read(argv[optind]) : NULL;
	if (command) handle(command);
	else {
		unsigned cmdlen=0;
		for (;;) {
			if (!f) putchar('$');
			if (1 > getline(&command, &cmdlen, f ? f : stdin)) break;

			handle(command);
		}
		if (ENABLE_FEATURE_CLEAN_UP) free(command);
	}

	return 1;
}
