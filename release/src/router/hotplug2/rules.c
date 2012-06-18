/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <regex.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mem_utils.h"
#include "filemap_utils.h"
#include "hotplug2.h"
#include "rules.h"


/**
 * Function supplementing 'mkdir -p'.
 *
 * @1 Path to be mkdir'd
 *
 * Returns: void
 */
static void mkdir_p(char *path) {
	char *ptr;
	struct stat statbuf;
	
	path = strdup(path);
	path = dirname(path);
	stat(path, &statbuf);
	/* All is well... */
	if (S_ISDIR(statbuf.st_mode)) {
		free(path);
		return;
	}
	
	for (ptr = path; ptr != NULL; ptr = strchr(ptr, '/')) {
		if (ptr == path) {
			ptr++;
			continue;
		}
		
		errno = 0;
		*ptr='\0';
		mkdir(path, 0755);
		*ptr='/';
		if (errno != 0 && errno != EEXIST)
			break;
		
		ptr++;
	}
	mkdir(path, 0755);
	free(path);
}

/**
 * Function supplementing 'rmdir -p'.
 *
 * @1 Path to be rmdir'd
 *
 * Returns: void
 */
static void rmdir_p(char *path) {
	char *ptr;
	
	path = strdup(path);
	ptr = path;
	while (ptr != NULL) {
		ptr = strrchr(path, '/');
		if (ptr == NULL)
			break;
		
		*ptr = '\0';
		
		if (rmdir(path))
			break;
	}
	free(path);
}

/**
 * Replaces all needles by a given value.
 *
 * @1 Haystack (which gets free'd in the function)
 * @2 Needle
 * @3 Needle replacement
 *
 * Returns: Newly allocated haysteck after replacement.
 */
static char *replace_str(char *hay, char *needle, char *replacement) {
        char *ptr, *start, *bptr, *buf;
        int occurences, j;
        size_t needle_len;
        size_t replacement_len;
        size_t haystack_len;

	if (replacement == NULL || *replacement=='\0')
		return hay;
	
        if (needle == NULL || *needle=='\0')
                return hay;
 
        occurences = 0;
        j = 0;
        for (ptr = hay; *ptr != '\0'; ++ptr) {
                if (needle[j] == *ptr) {
                        ++j;
                        if (needle[j] == '\0') {
                                *(ptr-j+1) = '\0'; // mark occurence
                                occurences++;
                                j = 0;
                        }
                } else {
			j=0;
		}
        }
       
        if (occurences <= 0)
                return hay;
       
        haystack_len = (size_t)(ptr - hay);
        replacement_len = strlen(replacement);
        needle_len = strlen(needle);
	
	buf = xmalloc(haystack_len + (replacement_len - needle_len) * occurences + 1);
	start = hay;
	ptr = hay;
	
	bptr = buf;
        while (occurences-- > 0) {
                while (*ptr != '\0')
                        ++ptr;

		if (ptr-start > 0) {
			memcpy(bptr, start, ptr - start);
			bptr +=ptr - start;
		}
		
		memcpy(bptr, replacement, replacement_len);
		bptr+=replacement_len;
		ptr += needle_len;
		start = ptr;
	}
	
	while (*ptr != '\0')
		++ptr;

	if (ptr-start > 0) {
		memcpy(bptr, start, ptr - start);
		bptr +=ptr - start;
	}
	*bptr='\0';

	free(hay);
	
        return buf;
}

/**
 * Trivial utility, figuring out whether a character is escaped or not.
 *
 * @1 Haystack
 * @2 Pointer to the character in question
 *
 * Returns: 1 if escaped, 0 otherwise
 */
static inline int isescaped(char *hay, char *ptr) {
	if (ptr <= hay)
		return 0;
	
	if (*(ptr-1) != '\\')
		return 0;
	
	return 1;
}

/**
 * Performs replacement of all keys by their value based on the hotplug
 * event structure. Keys are identified as strings %KEY%.
 *
 * @1 Haystack
 * @2 Hotplug event structure
 *
 * Returns: Newly allocated haystack (old is freed)
 */
static char *replace_key_by_value(char *hay, struct hotplug2_event_t *event) {
	char *sptr = hay, *ptr = hay;
	char *buf, *replacement;
	
	while ((sptr = strchr(sptr, '%')) != NULL) {
		ptr = strchr(sptr+1, '%');
		if (ptr != NULL) {
			buf = xmalloc(ptr - sptr + 2);
			buf[ptr - sptr + 1] = '\0';
			memcpy(buf, sptr, ptr - sptr + 1);
			
			buf[ptr - sptr] = '\0';
			replacement = get_hotplug2_value_by_key(event, &buf[1]);
			buf[ptr - sptr] = '%';
			
			if (replacement != NULL) {
				hay = replace_str(hay, buf, replacement);
				sptr = hay;
			} else {
				sptr++;
			}
			
			free(buf);
		} else {
			sptr++;
		}
	}
	
	hay = replace_str(hay, "%\\", "%");
	
	return hay;
}

/**
 * Obtains all information from hotplug event structure about a device node.
 * Creates the device node at a given path (expandable by keys) and with
 * given mode.
 *
 * @1 Hotplug event structure
 * @2 Path (may contain keys)
 * @3 Mode of the file
 *
 * Returns: 0 if success, non-zero otherwise
 */
static int make_dev_from_event(struct hotplug2_event_t *event, char *path, mode_t devmode) {
	char *subsystem, *major, *minor, *devpath;
	int rv = 1;
	
	major = get_hotplug2_value_by_key(event, "MAJOR");
	if (major == NULL)
		goto return_value;
	
	minor = get_hotplug2_value_by_key(event, "MINOR");
	if (minor == NULL)
		goto return_value;
	
	devpath = get_hotplug2_value_by_key(event, "DEVPATH");
	if (devpath == NULL)
		goto return_value;
	
	subsystem = get_hotplug2_value_by_key(event, "SUBSYSTEM");
	if (!strcmp(subsystem, "block"))
		devmode |= S_IFBLK;
	else
		devmode |= S_IFCHR;
	
	path = replace_key_by_value(path, event);
	mkdir_p(path);
	rv = mknod(path, devmode, makedev(atoi(major), atoi(minor)));

	/*
	 * Fixes an issue caused by devmode being modified by umask.
	 */
	chmod(path, devmode);

	free(path);
	
return_value:
	return rv;
}

/**
 * Execute an application without invoking a shell.
 *
 * @1 Hotplug event structure
 * @2 Path to the application, with expandable keys
 * @3 Argv for the application, with expandable keys
 *
 * Returns: Exit status of the application.
 */
static int exec_noshell(struct hotplug2_event_t *event, char *application, char **argv) {
	pid_t p;
	int i, status;
	
	p = fork();
	switch (p) {
		case -1:
			return -1;
		case 0:
			application = replace_key_by_value(strdup(application), event);
			for (i = 0; argv[i] != NULL; i++) {
				argv[i] = replace_key_by_value(argv[i], event);
			}
			execvp(application, argv);
			exit(127);
			break;
		default:
			if (waitpid(p, &status, 0) == -1)
				return -1;
			
			return WEXITSTATUS(status);
			break;
	}
}

/**
 * Execute an application while invoking a shell.
 *
 * @1 Hotplug event structure
 * @2 The application and all its arguments, with expandable keys
 *
 * Returns: Exit status of the application.
 */
static int exec_shell(struct hotplug2_event_t *event, char *application) {
	int rv;
	
	application = replace_key_by_value(strdup(application), event);
	rv = WEXITSTATUS(system(application));
	free(application);
	return rv;
}

/**
 * Create a symlink, with necessary parent directories.
 *
 * @1 Hotplug event structure
 * @2 Link target, with expandable keys
 * @3 Link name, with expandable keys
 *
 * Returns: return value of symlink()
 */
static int make_symlink(struct hotplug2_event_t *event, char *target, char *linkname) {
	int rv;
	
	target = replace_key_by_value(strdup(target), event);
	linkname = replace_key_by_value(strdup(linkname), event);
	
	mkdir_p(linkname);
	rv = symlink(target, linkname);
	
	free(target);
	free(linkname);
	
	return rv;
}

/**
 * Chmod a given file.
 *
 * @1 Hotplug event structure
 * @2 File name, with expandable keys
 * @3 Chmod value, with expandable keys
 *
 * Returns: return value of chmod()
 */
static int chmod_file(struct hotplug2_event_t *event, char *file, char *value) {
	int rv;

	file = replace_key_by_value(strdup(file), event);
	value = replace_key_by_value(strdup(value), event);

	rv = chmod(file, strtoul(value, 0, 8));

	free(file);
	free(value);

	return rv;
}


/**
 * Change owner or group of a given file.
 *
 * @1 Hotplug event structure
 * @2 Whether we chown or chgrp
 * @3 Filename, with expandable keys
 * @4 Group or user name, with expandable keys
 *
 * Returns: return value of chown()
 */
static int chown_chgrp(struct hotplug2_event_t *event, int action, char *file, char *param) {
	struct group *grp;
	struct passwd *pwd;
	int rv;

	file = replace_key_by_value(strdup(file), event);
	param = replace_key_by_value(strdup(param), event);

	rv = -1;

	switch (action) {
		case ACT_CHOWN:
			pwd = getpwnam(param);
			rv = chown(file, pwd->pw_uid, -1);
			break;
		case ACT_CHGRP:
			grp = getgrnam(param);
			rv = chown(file, -1, grp->gr_gid);
			break;
	}

	free(file);
	free(param);
	
	return rv;
}

/**
 * Prints all uevent keys.
 *
 * @1 Hotplug event structure
 *
 * Returns: void
 */
static void print_debug(struct hotplug2_event_t *event) {
	int i;

	for (i = 0; i < event->env_vars_c; i++)
		printf("%s=%s\n", event->env_vars[i].key, event->env_vars[i].value);
}

/**
 * Evaluates a condition according to a given hotplug event structure.
 *
 * @1 Hotplug event structure
 * @2 Condition to be evaluated
 *
 * Returns: 1 if match, 0 if no match, EVAL_NOT_AVAILABLE if unable to
 * perform evaluation
 */
int rule_condition_eval(struct hotplug2_event_t *event, struct condition_t *condition) {
	int rv;
	char *event_value = NULL;
	regex_t preg;
	
	event_value = get_hotplug2_value_by_key(event, condition->key);
	
	switch (condition->type) {
		case COND_MATCH_CMP:
		case COND_NMATCH_CMP:
			if (event_value == NULL)
				return EVAL_NOT_AVAILABLE;
			
			rv = strcmp(condition->value, event_value) ? EVAL_NOT_MATCH : EVAL_MATCH;
			if (condition->type == COND_NMATCH_CMP)
				rv = !rv;
			
			return rv;
		
		case COND_MATCH_RE:
		case COND_NMATCH_RE:
			if (event_value == NULL)
				return EVAL_NOT_AVAILABLE;
			
			regcomp(&preg, condition->value, REG_EXTENDED | REG_NOSUB);
			
			rv = regexec(&preg, event_value, 0, NULL, 0) ? EVAL_NOT_MATCH : EVAL_MATCH;
			if (condition->type == COND_NMATCH_RE)
				rv = !rv;
			
			regfree(&preg);
		
			return rv;
			
		case COND_MATCH_IS:
			if (!strcasecmp(condition->value, "set"))
				return event_value != NULL;
			
			if (!strcasecmp(condition->value, "unset"))
				return event_value == NULL;
	}
	
	return EVAL_NOT_AVAILABLE;
}

/**
 * Creates a "key=value" string from the given key and value
 *
 * @1 Key
 * @2 Value
 *
 * Returns: Newly allocated string in "key=value" form
 *
 */
static char* alloc_env(const char *key, const char *value) {
	size_t keylen, vallen;
	char *combined;

	keylen = strlen(key);
	vallen = strlen(value) + 1;

	combined = xmalloc(keylen + vallen + 1);
	memcpy(combined, key, keylen);
	combined[keylen] = '=';
	memcpy(&combined[keylen + 1], value, vallen);

	return combined;
}

/**
 * Executes a rule. Contains evaluation of all conditions prior
 * to execution.
 *
 * @1 Hotplug event structure
 * @2 The rule to be executed
 *
 * Returns: 0 if success, -1 if the whole event is to be 
 * discared, 1 if bail out of this particular rule was required
 */
int rule_execute(struct hotplug2_event_t *event, struct rule_t *rule) {
	int i, last_rv, res;
	char **env;
	
	for (i = 0; i < rule->conditions_c; i++) {
		if (rule_condition_eval(event, &(rule->conditions[i])) != EVAL_MATCH)
			return 0;
	}
	
	res = 0;
	last_rv = 0;
	
	env = xmalloc(sizeof(char *) * event->env_vars_c);
	for (i = 0; i < event->env_vars_c; i++) {
		env[i] = alloc_env(event->env_vars[i].key, event->env_vars[i].value);
		putenv(env[i]);
	}
	
	for (i = 0; i < rule->actions_c; i++) {
		switch (rule->actions[i].type) {
			case ACT_STOP_PROCESSING:
				res = 1;
				break;
			case ACT_STOP_IF_FAILED:
				if (last_rv != 0)
					res = 1;
				break;
			case ACT_NEXT_EVENT:
				res = -1;
				break;
			case ACT_NEXT_IF_FAILED:
				if (last_rv != 0)
					res = -1;
				break;
			case ACT_MAKE_DEVICE:
				last_rv = make_dev_from_event(event, rule->actions[i].parameter[0], strtoul(rule->actions[i].parameter[1], NULL, 0));
				break;
			case ACT_CHMOD:
				last_rv = chmod_file(event, rule->actions[i].parameter[0], rule->actions[i].parameter[1]);
				break;
			case ACT_CHOWN:
			case ACT_CHGRP:
				last_rv = chown_chgrp(event, rule->actions[i].type, rule->actions[i].parameter[0], rule->actions[i].parameter[1]);
				break;
			case ACT_SYMLINK:
				last_rv = make_symlink(event, rule->actions[i].parameter[0], rule->actions[i].parameter[1]);
				break;
			case ACT_RUN_SHELL:
				last_rv = exec_shell(event, rule->actions[i].parameter[0]);
				break;
			case ACT_RUN_NOSHELL:
				last_rv = exec_noshell(event, rule->actions[i].parameter[0], rule->actions[i].parameter);
				break;
			case ACT_SETENV:
				last_rv = setenv(rule->actions[i].parameter[0], rule->actions[i].parameter[1], 1);
				break;
			case ACT_REMOVE:
				last_rv = unlink(rule->actions[i].parameter[0]);
				rmdir_p(rule->actions[i].parameter[0]);
				break;
			case ACT_DEBUG:
				print_debug(event);
				last_rv = 0;
				break;
		}
		if (res != 0)
			break;
	}
	
	for (i = 0; i < event->env_vars_c; i++) {
		unsetenv(event->env_vars[i].key);
		free(env[i]);
	}
	free(env);
	
	return res;
}

/**
 * Sets the flags of the given rule.
 *
 * @1 Rule structure
 *
 * Returns: void
 */
void rule_flags(struct rule_t *rule) {
	int i;

	for (i = 0; i < rule->actions_c; i++) {
		switch (rule->actions[i].type) {
			case ACT_FLAG_NOTHROTTLE:
				rule->flags |= FLAG_NOTHROTTLE;
				break;
		}
	}
	
	return;
}

/**
 * Checks whether the given character should initiate
 * further parsing.
 *
 * @1 Character to examine
 *
 * Returns: 1 if it should, 0 otherwise
 */
static inline int isinitiator(int c) {
	switch (c) {
		case ',':
		case ';':
		case '{':
		case '}':
			return 1;
	}
	
	return 0;
}

/**
 * Appends a character to a buffer. Enlarges if necessary.
 *
 * @1 Pointer to the buffer
 * @2 Pointer to buffer size
 * @3 Pointer to last buffer character
 * @4 Appended character
 *
 * Returns: void
 */
static inline void add_buffer(char **buf, int *blen, int *slen, char c) {
	if (*slen + 1 >= *blen) {
		*blen = *blen + 64;
		*buf = xrealloc(*buf, *blen);
	}
	
	(*buf)[*slen] = c;
	(*buf)[*slen+1] = '\0';
	*slen += 1;
}

/**
 * Parses a string into a syntactically acceptable value.
 *
 * @1 Input string
 * @2 Pointer to the new position
 *
 * Returns: Newly allocated string.
 */
static char *rules_get_value(char *input, char **nptr) {
	int quotes = QUOTES_NONE;
	char *ptr = input;
	
	int blen, slen;
	char *buf;
	
	blen = slen = 0;
	buf = NULL;
	
	if (isinitiator(*ptr)) {
		add_buffer(&buf, &blen, &slen, *ptr);
		ptr++;
		goto return_value;
	}
	
	while (isspace(*ptr) && *ptr != '\0')
		ptr++;
	
	if (*ptr == '\0')
		return NULL;
	
	switch (*ptr) {
		case '"':
			quotes = QUOTES_DOUBLE;
			ptr++;
			break;
		case '\'':
			quotes = QUOTES_SINGLE;
			ptr++;
			break;
	}
	
	if (quotes != QUOTES_NONE) {
		while (quotes != QUOTES_NONE) {
			switch (*ptr) {
				case '\\':
					ptr++;
					add_buffer(&buf, &blen, &slen, *ptr);
					break;
				case '"':
					if (quotes == QUOTES_DOUBLE)
						quotes = QUOTES_NONE;
					break;
				case '\'':
					if (quotes == QUOTES_SINGLE)
						quotes = QUOTES_NONE;
					break;
				default:
					add_buffer(&buf, &blen, &slen, *ptr);
					break;
			}
			ptr++;
		}
	} else {
		while (!isspace(*ptr) && *ptr != '\0') {
			if (isinitiator(*ptr))
				break;
			
			if (*ptr == '\\')
				ptr++;
			
			add_buffer(&buf, &blen, &slen, *ptr);
			ptr++;
		}
	}
	
return_value:
	while (isspace(*ptr) && *ptr != '\0')
		ptr++;
	
	if (nptr != NULL)
		*nptr = ptr;
	
	return buf;
}

/**
 * Releases all memory associated with the ruleset. TODO: Make
 * the behavior same for all _free() functions, ie. either
 * release the given pointer itself or keep it, but do it
 * in all functions!
 *
 * @1 The ruleset to be freed
 *
 * Returns: void
 */
void rules_free(struct rules_t *rules) {
	int i, j, k;
	
	for (i = 0; i < rules->rules_c; i++) {
		for (j = 0; j < rules->rules[i].actions_c; j++) {
			if (rules->rules[i].actions[j].parameter != NULL) {
				for (k = 0; rules->rules[i].actions[j].parameter[k] != NULL; k++)
					free(rules->rules[i].actions[j].parameter[k]);
				free(rules->rules[i].actions[j].parameter);
			}
		}
		for (j = 0; j < rules->rules[i].conditions_c; j++) {
			free(rules->rules[i].conditions[j].key);
			free(rules->rules[i].conditions[j].value);
		}
		free(rules->rules[i].actions);
		free(rules->rules[i].conditions);
	}
	free(rules->rules);
}

/**
 * Includes a rule file.
 *
 * @1 Filename
 * @2 The ruleset structure
 *
 * Returns: 0 if success, -1 otherwise
 */
int rules_include(const char *filename, struct rules_t **return_rules) {
	struct filemap_t filemap;
	struct rules_t *rules;

	if (map_file(filename, &filemap)) {
		ERROR("rules parse","Unable to open/mmap rules file.");
		return -1;
	}
	
	rules = rules_from_config((char*)(filemap.map), *return_rules);
	if (rules == NULL) {
		ERROR("rules parse","Unable to parse rules file.");
		return -1;
	}
	*return_rules = rules;

	unmap_file(&filemap);

	return 0;
}

/**
 * Parses an entire file of rules.
 *
 * @1 The whole file in memory or mmap'd
 *
 * Returns: A newly allocated ruleset.
 */
struct rules_t *rules_from_config(char *input, struct rules_t *return_rules) {
	#define last_rule return_rules->rules[return_rules->rules_c - 1]
	int nested;
	int status;	
	int terminate;
	char *buf;

	/*
	 * TODO: cleanup
	 *
	 * BIIIG cleanup... Use callbacks for actions and for internal actions.
	 */
	
	int i, j;
	struct key_rec_t conditions[] = {	/*NOTE: We never have parameters for conditions. */
		{"is", 0, COND_MATCH_IS}, 
		{"==", 0, COND_MATCH_CMP}, 
		{"!=", 0, COND_NMATCH_CMP}, 
		{"~~", 0, COND_MATCH_RE}, 
		{"!~", 0, COND_NMATCH_RE},
		{NULL, 0, -1}
	};

	struct key_rec_t actions[] = {
		/*one line / one command*/
		{"run", 1, ACT_RUN_SHELL},
		{"exec", -1, ACT_RUN_NOSHELL},
		{"break", 0, ACT_STOP_PROCESSING},
		{"break_if_failed", 0, ACT_STOP_IF_FAILED},
		{"next", 0, ACT_NEXT_EVENT},
		{"next_if_failed", 0, ACT_NEXT_IF_FAILED},
		{"chown", 2, ACT_CHOWN},
		{"chmod", 2, ACT_CHMOD},
		{"chgrp", 2, ACT_CHGRP},
		{"setenv", 2, ACT_SETENV},
		{"remove", 1, ACT_REMOVE},
		{"nothrottle", 0, ACT_FLAG_NOTHROTTLE},
		{"printdebug", 0, ACT_DEBUG},
		/*symlink*/
		{"symlink", 2, ACT_SYMLINK},
		{"softlink", 2, ACT_SYMLINK},
		/*makedev*/
		{"mknod", 2, ACT_MAKE_DEVICE},
		{"makedev", 2, ACT_MAKE_DEVICE},
		{NULL, 0, -1}
	};

	/*
	 * A little trick for inclusion.
	 */
	if (return_rules == NULL) {
		return_rules = xmalloc(sizeof(struct rules_t));
		return_rules->rules_c = 1;
		return_rules->rules = xmalloc(sizeof(struct rule_t) * return_rules->rules_c);
		nested = 0;
	} else {
		nested = 1;
	}

	status = STATUS_KEY;
	
	last_rule.actions = NULL;
	last_rule.actions_c = 0;
	last_rule.conditions = NULL;
	last_rule.conditions_c = 0;
	
	terminate = 0;
	do {
		buf = rules_get_value(input, &input);
		if (buf == NULL) {
			ERROR("rules_get_value", "Malformed rule - unable to read!");
			terminate = 1;
			break;
		}
		
		if (buf[0] == '#') {
			/* Skip to next line */
			while (*input != '\0' && *input != '\n')
				input++;

			free(buf);
			continue;
		} else if (buf[0] == '$') {
			buf++;

			/*
			 * Warning, hack ahead...
			 */
			if (!strcmp("include", buf)) {
				buf = rules_get_value(input, &input);
				if (rules_include(buf, &return_rules)) {
					ERROR("rules_include", "Unable to include ruleset '%s'!", buf);
				}
			}

			free(buf);
			continue;
		}

		switch (status) {
			case STATUS_KEY:
				last_rule.conditions_c++;
				last_rule.conditions = xrealloc(last_rule.conditions, sizeof(struct condition_t) * last_rule.conditions_c);
				last_rule.conditions[last_rule.conditions_c-1].key = strdup(buf);
				
				status = STATUS_CONDTYPE;
				break;
			case STATUS_CONDTYPE:
				last_rule.conditions[last_rule.conditions_c-1].type = -1;
				
				for (i = 0; conditions[i].key != NULL; i++) {
					if (!strcmp(conditions[i].key, buf)) {
						last_rule.conditions[last_rule.conditions_c-1].type = conditions[i].type;
						break;
					}
				}
				
				if (last_rule.conditions[last_rule.conditions_c-1].type == -1) {
					ERROR("rules_get_value / status / condtype", "Malformed rule - unknown condition type.");
					terminate = 1;
				}
				
				status = STATUS_VALUE;
				break;
			case STATUS_VALUE:
				last_rule.conditions[last_rule.conditions_c-1].value = strdup(buf);
			
				status = STATUS_INITIATOR;
				break;
			case STATUS_INITIATOR:
				if (!strcmp(buf, ",") || !strcmp(buf, ";")) {
					status = STATUS_KEY;
				} else if (!strcmp(buf, "{")) {
					status = STATUS_ACTION;
				} else {
					ERROR("rules_get_value / status / initiator", "Malformed rule - unknown initiator.");
					terminate = 1;
				}
				break;
			case STATUS_ACTION:
				if (!strcmp(buf, "}")) {
					status = STATUS_KEY;
					return_rules->rules_c++;
					return_rules->rules = xrealloc(return_rules->rules, sizeof(struct rule_t) * return_rules->rules_c);
					
					last_rule.actions = NULL;
					last_rule.actions_c = 0;
					last_rule.conditions = NULL;
					last_rule.conditions_c = 0;
					break;
				}
				
				last_rule.actions_c++;
				last_rule.actions = xrealloc(last_rule.actions, sizeof(struct action_t) * last_rule.actions_c);
				last_rule.actions[last_rule.actions_c-1].parameter = NULL;
				last_rule.actions[last_rule.actions_c-1].type = -1;
				
				for (i = 0; actions[i].key != NULL; i++) {
					if (!strcmp(actions[i].key, buf)) {
						last_rule.actions[last_rule.actions_c-1].type = actions[i].type;
						break;
					}
				}
				
				if (last_rule.actions[last_rule.actions_c-1].type == -1) {
					ERROR("rules_get_value / status / action", "Malformed rule - unknown action: %s.", buf);
					terminate = 1;
				}
				
				if (actions[i].param > 0) {
					last_rule.actions[last_rule.actions_c-1].parameter = xmalloc(sizeof(char*) * (actions[i].param + 1));
					last_rule.actions[last_rule.actions_c-1].parameter[actions[i].param] = NULL;
					
					for (j = 0; j < actions[i].param; j++) {
						last_rule.actions[last_rule.actions_c-1].parameter[j] = rules_get_value(input, &input);
						if (!strcmp(last_rule.actions[last_rule.actions_c-1].parameter[j], "}")) {
							ERROR("rules_get_value / status / action", "Malformed rule - not enough parameters passed.");
							status = STATUS_KEY;
							break;
						}
						last_rule.actions[last_rule.actions_c-1].parameter[j] = replace_str(last_rule.actions[last_rule.actions_c-1].parameter[j], "\\}", "}");
					}
				} else if (actions[i].param == -1) {
					j = 0;
					last_rule.actions[last_rule.actions_c-1].parameter = xmalloc(sizeof(char*) * (j + 1));
					last_rule.actions[last_rule.actions_c-1].parameter[j] = rules_get_value(input, &input);
					while (last_rule.actions[last_rule.actions_c-1].parameter[j] != NULL) {
						if (!strcmp(last_rule.actions[last_rule.actions_c-1].parameter[j], ";")) {
							break;
						}
						if (!strcmp(last_rule.actions[last_rule.actions_c-1].parameter[j], "}")) {
							ERROR("rules_get_value / status / action", "Malformed rule - missing parameter terminator ';'.");
							status = STATUS_KEY;
							break;
						}
						if (last_rule.actions[last_rule.actions_c-1].parameter[j][0] == '\0') {
							ERROR("rules_get_value / status / action", "Malformed rule - missing parameter terminator ';'.");
							status = STATUS_KEY;
							break;
						}
						last_rule.actions[last_rule.actions_c-1].parameter[j] = replace_str(last_rule.actions[last_rule.actions_c-1].parameter[j], "\\}", "}");
						last_rule.actions[last_rule.actions_c-1].parameter[j] = replace_str(last_rule.actions[last_rule.actions_c-1].parameter[j], "\\;", ";");
						
						j++;
						last_rule.actions[last_rule.actions_c-1].parameter = xrealloc(last_rule.actions[last_rule.actions_c-1].parameter, sizeof(char*) * (j + 1));
						last_rule.actions[last_rule.actions_c-1].parameter[j]  = rules_get_value(input, &input);
					}
					free(last_rule.actions[last_rule.actions_c-1].parameter[j]);
					last_rule.actions[last_rule.actions_c-1].parameter[j] = NULL;
				}
				
				if (status == STATUS_KEY) {
					return_rules->rules_c++;
					return_rules->rules = xrealloc(return_rules->rules, sizeof(struct rule_t) * return_rules->rules_c);
					
					last_rule.actions = NULL;
					last_rule.actions_c = 0;
					last_rule.conditions = NULL;
					last_rule.conditions_c = 0;
				}
				break;
		}
		
		free(buf);
	} while (*input != '\0' && !terminate);
	
	if (!terminate) {
		/* A little bit hacky cleanup */
		if (!nested)
			return_rules->rules_c--;
		return return_rules;
	} else {
		/*
		 * We don't want to cleanup if we're nested.
		 */
		if (!nested) {
			rules_free(return_rules);
			free(return_rules);
		}

		return NULL;
	}
}
