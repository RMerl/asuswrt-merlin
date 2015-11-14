/*
 * subst.c --- substitution program
 *
 * Subst is used as a quicky program to do @ substitutions
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define HAVE_SYS_TIME_H
#endif
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif


struct subst_entry {
	char *name;
	char *value;
	struct subst_entry *next;
};

static struct subst_entry *subst_table = 0;

static int add_subst(char *name, char *value)
{
	struct subst_entry	*ent = 0;

	ent = (struct subst_entry *) malloc(sizeof(struct subst_entry));
	if (!ent)
		goto fail;
	ent->name = (char *) malloc(strlen(name)+1);
	if (!ent->name)
		goto fail;
	ent->value = (char *) malloc(strlen(value)+1);
	if (!ent->value)
		goto fail;
	strcpy(ent->name, name);
	strcpy(ent->value, value);
	ent->next = subst_table;
	subst_table = ent;
	return 0;
fail:
	if (ent) {
		free(ent->name);
		free(ent);
	}
	return ENOMEM;
}

static struct subst_entry *fetch_subst_entry(char *name)
{
	struct subst_entry *ent;

	for (ent = subst_table; ent; ent = ent->next) {
		if (strcmp(name, ent->name) == 0)
			break;
	}
	return ent;
}

/*
 * Given the starting and ending position of the replacement name,
 * check to see if it is valid, and pull it out if it is.
 */
static char *get_subst_symbol(const char *begin, size_t len, char prefix)
{
	static char replace_name[128];
	char *cp, *start;

	start = replace_name;
	if (prefix)
		*start++ = prefix;

	if (len > sizeof(replace_name)-2)
		return NULL;
	memcpy(start, begin, len);
	start[len] = 0;

	/*
	 * The substitution variable must all be in the of [0-9A-Za-z_].
	 * If it isn't, this must be an invalid symbol name.
	 */
	for (cp = start; *cp; cp++) {
		if (!(*cp >= 'a' && *cp <= 'z') &&
		    !(*cp >= 'A' && *cp <= 'Z') &&
		    !(*cp >= '0' && *cp <= '9') &&
		    !(*cp == '_'))
			return NULL;
	}
	return (replace_name);
}

static void replace_string(char *begin, char *end, char *newstr)
{
	int	replace_len, len;

	replace_len = strlen(newstr);
	len = end - begin;
	if (replace_len == 0)
		memmove(begin, end+1, strlen(end)+1);
	else if (replace_len != len+1)
		memmove(end+(replace_len-len-1), end,
			strlen(end)+1);
	memcpy(begin, newstr, replace_len);
}

static void substitute_line(char *line)
{
	char	*ptr, *name_ptr, *end_ptr;
	struct subst_entry *ent;
	char	*replace_name;
	size_t	len;

	/*
	 * Expand all @FOO@ substitutions
	 */
	ptr = line;
	while (ptr) {
		name_ptr = strchr(ptr, '@');
		if (!name_ptr)
			break;	/* No more */
		if (*(++name_ptr) == '@') {
			/*
			 * Handle tytso@@mit.edu --> tytso@mit.edu
			 */
			memmove(name_ptr-1, name_ptr, strlen(name_ptr)+1);
			ptr = name_ptr+1;
			continue;
		}
		end_ptr = strchr(name_ptr, '@');
		if (!end_ptr)
			break;
		len = end_ptr - name_ptr;
		replace_name = get_subst_symbol(name_ptr, len, 0);
		if (!replace_name) {
			ptr = name_ptr;
			continue;
		}
		ent = fetch_subst_entry(replace_name);
		if (!ent) {
			fprintf(stderr, "Unfound expansion: '%s'\n",
				replace_name);
			ptr = end_ptr + 1;
			continue;
		}
#if 0
		fprintf(stderr, "Replace name = '%s' with '%s'\n",
		       replace_name, ent->value);
#endif
		ptr = name_ptr-1;
		replace_string(ptr, end_ptr, ent->value);
		if ((ent->value[0] == '@') &&
		    (strlen(replace_name) == strlen(ent->value)-2) &&
		    !strncmp(replace_name, ent->value+1,
			     strlen(ent->value)-2))
			/* avoid an infinite loop */
			ptr += strlen(ent->value);
	}
	/*
	 * Now do a second pass to expand ${FOO}
	 */
	ptr = line;
	while (ptr) {
		name_ptr = strchr(ptr, '$');
		if (!name_ptr)
			break;	/* No more */
		if (*(++name_ptr) != '{') {
			ptr = name_ptr;
			continue;
		}
		name_ptr++;
		end_ptr = strchr(name_ptr, '}');
		if (!end_ptr)
			break;
		len = end_ptr - name_ptr;
		replace_name = get_subst_symbol(name_ptr, len, '$');
		if (!replace_name) {
			ptr = name_ptr;
			continue;
		}
		ent = fetch_subst_entry(replace_name);
		if (!ent) {
			ptr = end_ptr + 1;
			continue;
		}
#if 0
		fprintf(stderr, "Replace name = '%s' with '%s'\n",
		       replace_name, ent->value);
#endif
		ptr = name_ptr-2;
		replace_string(ptr, end_ptr, ent->value);
	}
}

static void parse_config_file(FILE *f)
{
	char	line[2048];
	char	*cp, *ptr;

	while (!feof(f)) {
		memset(line, 0, sizeof(line));
		if (fgets(line, sizeof(line), f) == NULL)
			break;
		/*
		 * Strip newlines and comments.
		 */
		cp = strchr(line, '\n');
		if (cp)
			*cp = 0;
		cp = strchr(line, '#');
		if (cp)
			*cp = 0;
		/*
		 * Skip trailing and leading whitespace
		 */
		for (cp = line + strlen(line) - 1; cp >= line; cp--) {
			if (*cp == ' ' || *cp == '\t')
				*cp = 0;
			else
				break;
		}
		cp = line;
		while (*cp && isspace(*cp))
			cp++;
		ptr = cp;
		/*
		 * Skip empty lines
		 */
		if (*ptr == 0)
			continue;
		/*
		 * Ignore future extensions
		 */
		if (*ptr == '@')
			continue;
		/*
		 * Parse substitutions
		 */
		for (cp = ptr; *cp; cp++)
			if (isspace(*cp))
				break;
		*cp = 0;
		for (cp++; *cp; cp++)
			if (!isspace(*cp))
				break;
#if 0
		printf("Substitute: '%s' for '%s'\n", ptr, cp ? cp : "<NULL>");
#endif
		add_subst(ptr, cp);
	}
}

/*
 * Return 0 if the files are different, 1 if the files are the same.
 */
static int compare_file(FILE *old_f, FILE *new_f)
{
	char	oldbuf[2048], newbuf[2048], *oldcp, *newcp;
	int	retval;

	while (1) {
		oldcp = fgets(oldbuf, sizeof(oldbuf), old_f);
		newcp = fgets(newbuf, sizeof(newbuf), new_f);
		if (!oldcp && !newcp) {
			retval = 1;
			break;
		}
		if (!oldcp || !newcp || strcmp(oldbuf, newbuf)) {
			retval = 0;
			break;
		}
	}
	return retval;
}

void set_utimes(const char *filename, int fd, const struct timeval times[2])
{
#ifdef HAVE_FUTIMES
	if (futimes(fd, times) < 0)
		perror("futimes");
#elif HAVE_UTIMES
	if (utimes(filename, times) < 0)
		perror("utimes");
#else
	struct utimbuf ut;

	ut.actime = times[0].tv_sec;
	ut.modtime = times[1].tv_sec;
	if (utime(filename, &ut) < 0)
		perror("utime");
#endif
}


int main(int argc, char **argv)
{
	char	line[2048];
	int	c;
	int	fd;
	FILE	*in, *out, *old = NULL;
	char	*outfn = NULL, *newfn = NULL;
	int	verbose = 0;
	int	adjust_timestamp = 0;
	int	got_atime = 0;
	struct stat stbuf;
	struct timeval tv[2];

	while ((c = getopt (argc, argv, "f:tv")) != EOF) {
		switch (c) {
		case 'f':
			in = fopen(optarg, "r");
			if (!in) {
				perror(optarg);
				exit(1);
			}
			parse_config_file(in);
			fclose(in);
			break;
		case 't':
			adjust_timestamp++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			fprintf(stderr, "%s: [-f config-file] [file]\n",
				argv[0]);
			break;
		}
	}
	if (optind < argc) {
		in = fopen(argv[optind], "r");
		if (!in) {
			perror(argv[optind]);
			exit(1);
		}
		optind++;
	} else
		in = stdin;

	if (optind < argc) {
		outfn = argv[optind];
		newfn = (char *) malloc(strlen(outfn)+20);
		if (!newfn) {
			fprintf(stderr, "Memory error!  Exiting.\n");
			exit(1);
		}
		strcpy(newfn, outfn);
		strcat(newfn, ".new");
		fd = open(newfn, O_CREAT|O_TRUNC|O_RDWR, 0444);
		if (fd < 0) {
			perror(newfn);
			exit(1);
		}
		out = fdopen(fd, "w+");
		if (!out) {
			perror("fdopen");
			exit(1);
		}

		fd = open(outfn, O_RDONLY);
		if (fd > 0) {
			/* save the original atime, if possible */
			if (fstat(fd, &stbuf) == 0) {
#if HAVE_STRUCT_STAT_ST_ATIM
				tv[0].tv_sec = stbuf.st_atim.tv_sec;
				tv[0].tv_usec = stbuf.st_atim.tv_nsec / 1000;
#else
				tv[0].tv_sec = stbuf.st_atime;
				tv[0].tv_usec = 0;
#endif
				got_atime = 1;
			}
			old = fdopen(fd, "r");
			if (!old)
				close(fd);
		}
	} else {
		out = stdout;
		outfn = 0;
	}

	while (!feof(in)) {
		if (fgets(line, sizeof(line), in) == NULL)
			break;
		substitute_line(line);
		fputs(line, out);
	}
	fclose(in);
	if (outfn) {
		fflush(out);
		rewind(out);
		if (old && compare_file(old, out)) {
			if (verbose)
				printf("No change, keeping %s.\n", outfn);
			if (adjust_timestamp) {
				if (verbose)
					printf("Updating modtime for %s\n", outfn);
				if (gettimeofday(&tv[1], NULL) < 0) {
					perror("gettimeofday");
					exit(1);
				}
				if (got_atime == 0)
					tv[0] = tv[1];
				else if (verbose)
					printf("Using original atime\n");
				set_utimes(outfn, fileno(old), tv);
			}
			fclose(out);
			if (unlink(newfn) < 0)
				perror("unlink");
		} else {
			if (verbose)
				printf("Creating or replacing %s.\n", outfn);
			fclose(out);
			if (old)
				fclose(old);
			old = NULL;
			if (rename(newfn, outfn) < 0) {
				perror("rename");
				exit(1);
			}
		}
	}
	if (old)
		fclose(old);
	return (0);
}


