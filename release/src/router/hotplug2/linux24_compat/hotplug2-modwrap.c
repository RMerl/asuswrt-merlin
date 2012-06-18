/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <linux/types.h>
#include <linux/input.h>

#include "../hotplug2.h"
#include "../mem_utils.h"
#include "../parser_utils.h"
#include "../filemap_utils.h"

#define MODULES_PATH		"/lib/modules/"
#define MODULES_ALIAS		"modules.alias"

/**
 * A simple fork/exec wrapper
 *
 * @1 Complete argv, including app path
 *
 * Returns: -1 if error, children return value otherwise
 */
int execute(char **argv) {
	pid_t p;
	int status;
	
	p = fork();
	switch (p) {
		case -1:
			return -1;
		case 0:
			execvp(argv[0], argv);
			exit(1);
			break;
		default:
			waitpid(p, &status, 0);
			break;
	}

	return WEXITSTATUS(status);
}

int main(int argc, char *argv[]) {
	struct utsname unamebuf;
	struct filemap_t aliasmap;
	char *line, *nline, *nptr;
	char *token;
	char *filename;
	
	char *cur_alias, *match_alias, *module;
	
	if (argc < 2) {
		fprintf(stderr, "Usage: hotplug2-modwrap [options for modprobe] <alias>\n");
	}
	
	match_alias = strdup(argv[argc - 1]);
	
	/*
	 * If we can't do uname, we're absolutely screwed and there's no
	 * sense thinking twice about anything.
	 */
	if (uname(&unamebuf)) {
		ERROR("uname", "Unable to perform uname: %s.", strerror(errno));
		return 1;
	}
	
	/*
	 * We allow setting the modprobe command to an arbitrary value.
	 *
	 * The whole trick lies in executing modprobe with exactly the
	 * same argv as this app was executed, except we use a different
	 * argv[0] (application path) and argv[argc-1] (we substitute
	 * the given modalias by the matching module name)
	 */
	argv[0] = getenv("MODPROBE_COMMAND");
	if (argv[0] == NULL)
		argv[0] = "/sbin/modprobe";

	/*
	 * Compose a path, /lib/modules/`uname -r`/modules.alias
	 *
	 * "/lib/modules/" + "/" + "\0" 
	 */
	filename = xmalloc(strlen(MODULES_PATH) + strlen(unamebuf.release) + strlen(MODULES_ALIAS));
	strcpy(filename, MODULES_PATH);
	strcat(filename, unamebuf.release);
	strcat(filename, MODULES_ALIAS);
	
	if (map_file(filename, &aliasmap)) {
		ERROR("map_file", "Unable to map file: `%s'.", filename);
		free(filename);
		free(match_alias);
		return 1;
	}
	
	/*
	 * Read all the aliases, match them against given parameter.
	 */
	nptr = aliasmap.map;
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		/*
		 * We want aliases only
		 */
		token = dup_token(nline, &nline, isspace);
		if (!token || strcmp(token, "alias")) {
			free(token);
			free(line);
			continue;
		}
		free(token);
		
		/*
		 * It's an alias, so fetch it
		 */
		cur_alias = dup_token(nline, &nline, isspace);
		if (!cur_alias) {
			free(line);
			continue;
		}
		
		/*
		 * And now we get the module name
		 */
		module = dup_token(nline, &nline, isspace);
		if (!module) {
			free(line);
			free(cur_alias);
			continue;
		}
		
		/*
		 * If we match, we do the modalias->module name
		 * substitution as described above and execute.
		 */
		if (!fnmatch(cur_alias, match_alias, 0)) {
			argv[argc - 1] = module;
			if (execute(argv)) {
				ERROR("execute", "Error during exection of: `%s'.", argv[0]);
			}
		}
		
		free(cur_alias);
		free(module);
		free(line);
	}

	/*
	 * Perhaps we didn't match anything, so we might've been given
	 * a module name instead of a modalias. Try to modprobe it
	 * right away.
	 */
	if (strcmp(argv[argc - 1], match_alias) == 0) {
		if (execute(argv)) {
			ERROR("execute", "Error during exection of: `%s'.", argv[0]);
		}
	}	
	
	free(filename);
	free(match_alias);
	unmap_file(&aliasmap);
	
	return 0;
}
