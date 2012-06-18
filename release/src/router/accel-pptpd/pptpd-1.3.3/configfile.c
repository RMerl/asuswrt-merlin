/*
 * configfile.c
 *
 * Methods for accessing the PPTPD config file and searching for
 * PPTPD keywords.
 *
 * $Id: configfile.c,v 1.2 2004/04/22 10:48:16 quozl Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "defaults.h"
#include "configfile.h"
#include "our_syslog.h"

/* Local function prototypes */
static FILE *open_config_file(char *filename);
static void close_config_file(FILE * file);

/*
 * read_config_file
 *
 * This method opens up the file specified by 'filename' and searches
 * through the file for 'keyword'. If 'keyword' is found any string
 * following it is stored in 'value'.
 *
 * args: filename (IN) - config filename
 *       keyword (IN) - word to search for in config file
 *       value (OUT) - value of keyword
 *
 * retn: -1 on error, 0 if keyword not found, 1 on value success
 */
int read_config_file(char *filename, char *keyword, char *value)
{
	FILE *in;
	int len = 0, keyword_len = 0;
	int foundit = 0;

	char *buff_ptr;
	char buffer[MAX_CONFIG_STRING_SIZE];

	*value = '\0';
	buff_ptr = buffer;
	keyword_len = strlen(keyword);

	in = open_config_file(filename);
	if (in == NULL) {
		/* Couldn't find config file, or permission denied */
		return -1;
	}
	while ((fgets(buffer, MAX_CONFIG_STRING_SIZE - 1, in)) != NULL) {
		/* ignore long lines */
		if (buffer[(len = strlen(buffer)) - 1] != '\n') {
			syslog(LOG_ERR, "Long config file line ignored.");
			do
				if (!fgets(buffer, MAX_CONFIG_STRING_SIZE - 1, in)) break;
			while (buffer[strlen(buffer) - 1] != '\n');
			continue;
		}

		len--;			/* For the NL at the end */
		while (--len >= 0)
			if (buffer[len] != ' ' && buffer[len] != '\t')
				break;

		len++;
		buffer[len] = '\0';

		buff_ptr = buffer;

		/* Short-circuit blank lines and comments */
		if (!len || *buff_ptr == '#')
			continue;

		/* Non-blank lines starting with a space are an error */

		if (*buff_ptr == ' ' || *buff_ptr == '\t') {
			syslog(LOG_ERR, "Config file line starts with a space: %s", buff_ptr);
			continue;
		}

		/* At this point we have a line trimmed for trailing spaces. */
		/* Now we need to check if the keyword matches, and if so */
		/* then get the value (if any). */

		/* Check if it's the right keyword */

		do {
			if (*buff_ptr == ' ' || *buff_ptr == '\t')
				break;
		} while (*++buff_ptr);

		len = buff_ptr - buffer;
		if (len == keyword_len && !strncmp(buffer, keyword, len)) {
			foundit++;
			break;
		}
	}

	close_config_file(in);

	if (foundit) {
		/* Right keyword, now get the value (if any) */

		do {
			if (*buff_ptr != ' ' && *buff_ptr != '\t')
				break;
			
		} while (*++buff_ptr);

		strcpy(value, buff_ptr);
		return 1;
	} else {
		/* didn't find it - better luck next time */
		return 0;
	}
}

/*
 * open_config_file
 *
 * Opens up the PPTPD config file for reading.
 *
 * args: filename - the config filename (eg. '/etc/pptpd.conf')
 *
 * retn: NULL on error, file descriptor on success
 *
 */
static FILE *open_config_file(char *filename)
{
	FILE *in;
	static int first = 1;

	if ((in = fopen(filename, "r")) == NULL) {
		/* Couldn't open config file */
		if (first) {
			perror(filename);
			first = 0;
		}
		return NULL;
	}
	return in;
}

/*
 * close_config_file
 *
 * Closes the PPTPD config file descriptor
 *
 */
static void close_config_file(FILE * in)
{
	fclose(in);
}
