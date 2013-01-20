/*
 * read.c - read the blkid cache from disk, to avoid scanning all devices
 *
 * Copyright (C) 2001, 2003 Theodore Y. Ts'o
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#define _XOPEN_SOURCE 600 /* for inclusion of strtoull */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "blkidP.h"
#include "uuid/uuid.h"

#ifdef HAVE_STRTOULL
#define STRTOULL strtoull /* defined in stdlib.h if you try hard enough */
#else
/* FIXME: need to support real strtoull here */
#define STRTOULL strtoul
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef TEST_PROGRAM
#define blkid_debug_dump_dev(dev)	(debug_dump_dev(dev))
static void debug_dump_dev(blkid_dev dev);
#endif

/*
 * File format:
 *
 *	<device [<NAME="value"> ...]>device_name</device>
 *
 *	The following tags are required for each entry:
 *	<ID="id">	unique (within this file) ID number of this device
 *	<TIME="time">	(ascii time_t) time this entry was last read from disk
 *	<TYPE="type">	(detected) type of filesystem/data for this partition
 *
 *	The following tags may be present, depending on the device contents
 *	<LABEL="label">	(user supplied) label (volume name, etc)
 *	<UUID="uuid">	(generated) universally unique identifier (serial no)
 */

static char *skip_over_blank(char *cp)
{
	while (*cp && isspace(*cp))
		cp++;
	return cp;
}

static char *skip_over_word(char *cp)
{
	char ch;

	while ((ch = *cp)) {
		/* If we see a backslash, skip the next character */
		if (ch == '\\') {
			cp++;
			if (*cp == '\0')
				break;
			cp++;
			continue;
		}
		if (isspace(ch) || ch == '<' || ch == '>')
			break;
		cp++;
	}
	return cp;
}

static char *strip_line(char *line)
{
	char	*p;

	line = skip_over_blank(line);

	p = line + strlen(line) - 1;

	while (*line) {
		if (isspace(*p))
			*p-- = '\0';
		else
			break;
	}

	return line;
}

#if 0
static char *parse_word(char **buf)
{
	char *word, *next;

	word = *buf;
	if (*word == '\0')
		return NULL;

	word = skip_over_blank(word);
	next = skip_over_word(word);
	if (*next) {
		char *end = next - 1;
		if (*end == '"' || *end == '\'')
			*end = '\0';
		*next++ = '\0';
	}
	*buf = next;

	if (*word == '"' || *word == '\'')
		word++;
	return word;
}
#endif

/*
 * Start parsing a new line from the cache.
 *
 * line starts with "<device" return 1 -> continue parsing line
 * line starts with "<foo", empty, or # return 0 -> skip line
 * line starts with other, return -BLKID_ERR_CACHE -> error
 */
static int parse_start(char **cp)
{
	char *p;

	p = strip_line(*cp);

	/* Skip comment or blank lines.  We can't just NUL the first '#' char,
	 * in case it is inside quotes, or escaped.
	 */
	if (*p == '\0' || *p == '#')
		return 0;

	if (!strncmp(p, "<device", 7)) {
		DBG(DEBUG_READ, printf("found device header: %8s\n", p));
		p += 7;

		*cp = p;
		return 1;
	}

	if (*p == '<')
		return 0;

	return -BLKID_ERR_CACHE;
}

/* Consume the remaining XML on the line (cosmetic only) */
static int parse_end(char **cp)
{
	*cp = skip_over_blank(*cp);

	if (!strncmp(*cp, "</device>", 9)) {
		DBG(DEBUG_READ, printf("found device trailer %9s\n", *cp));
		*cp += 9;
		return 0;
	}

	return -BLKID_ERR_CACHE;
}

/*
 * Allocate a new device struct with device name filled in.  Will handle
 * finding the device on lines of the form:
 * <device foo=bar>devname</device>
 * <device>devname<foo>bar</foo></device>
 */
static int parse_dev(blkid_cache cache, blkid_dev *dev, char **cp)
{
	char *start, *tmp, *end, *name;
	int ret;

	if ((ret = parse_start(cp)) <= 0)
		return ret;

	start = tmp = strchr(*cp, '>');
	if (!start) {
		DBG(DEBUG_READ,
		    printf("blkid: short line parsing dev: %s\n", *cp));
		return -BLKID_ERR_CACHE;
	}
	start = skip_over_blank(start + 1);
	end = skip_over_word(start);

	DBG(DEBUG_READ, printf("device should be %*s\n",
			       (int)(end - start), start));

	if (**cp == '>')
		*cp = end;
	else
		(*cp)++;

	*tmp = '\0';

	if (!(tmp = strrchr(end, '<')) || parse_end(&tmp) < 0) {
		DBG(DEBUG_READ,
		    printf("blkid: missing </device> ending: %s\n", end));
	} else if (tmp)
		*tmp = '\0';

	if (end - start <= 1) {
		DBG(DEBUG_READ, printf("blkid: empty device name: %s\n", *cp));
		return -BLKID_ERR_CACHE;
	}

	name = blkid_strndup(start, end-start);
	if (name == NULL)
		return -BLKID_ERR_MEM;

	DBG(DEBUG_READ, printf("found dev %s\n", name));

	if (!(*dev = blkid_get_dev(cache, name, BLKID_DEV_CREATE))) {
		free(name);
		return -BLKID_ERR_MEM;
	}

	free(name);
	return 1;
}

/*
 * Extract a tag of the form NAME="value" from the line.
 */
static int parse_token(char **name, char **value, char **cp)
{
	char *end;

	if (!name || !value || !cp)
		return -BLKID_ERR_PARAM;

	if (!(*value = strchr(*cp, '=')))
		return 0;

	**value = '\0';
	*name = strip_line(*cp);
	*value = skip_over_blank(*value + 1);

	if (**value == '"') {
		end = strchr(*value + 1, '"');
		if (!end) {
			DBG(DEBUG_READ,
			    printf("unbalanced quotes at: %s\n", *value));
			*cp = *value;
			return -BLKID_ERR_CACHE;
		}
		(*value)++;
		*end = '\0';
		end++;
	} else {
		end = skip_over_word(*value);
		if (*end) {
			*end = '\0';
			end++;
		}
	}
	*cp = end;

	return 1;
}

/*
 * Extract a tag of the form <NAME>value</NAME> from the line.
 */
/*
static int parse_xml(char **name, char **value, char **cp)
{
	char *end;

	if (!name || !value || !cp)
		return -BLKID_ERR_PARAM;

	*name = strip_line(*cp);

	if ((*name)[0] != '<' || (*name)[1] == '/')
		return 0;

	FIXME: finish this.
}
*/

/*
 * Extract a tag from the line.
 *
 * Return 1 if a valid tag was found.
 * Return 0 if no tag found.
 * Return -ve error code.
 */
static int parse_tag(blkid_cache cache, blkid_dev dev, char **cp)
{
	char *name;
	char *value;
	int ret;

	if (!cache || !dev)
		return -BLKID_ERR_PARAM;

	if ((ret = parse_token(&name, &value, cp)) <= 0 /* &&
	    (ret = parse_xml(&name, &value, cp)) <= 0 */)
		return ret;

	/* Some tags are stored directly in the device struct */
	if (!strcmp(name, "DEVNO"))
		dev->bid_devno = STRTOULL(value, 0, 0);
	else if (!strcmp(name, "PRI"))
		dev->bid_pri = strtol(value, 0, 0);
	else if (!strcmp(name, "TIME"))
		dev->bid_time = STRTOULL(value, 0, 0);
	else
		ret = blkid_set_tag(dev, name, value, strlen(value));

	DBG(DEBUG_READ, printf("    tag: %s=\"%s\"\n", name, value));

	return ret < 0 ? ret : 1;
}

/*
 * Parse a single line of data, and return a newly allocated dev struct.
 * Add the new device to the cache struct, if one was read.
 *
 * Lines are of the form <device [TAG="value" ...]>/dev/foo</device>
 *
 * Returns -ve value on error.
 * Returns 0 otherwise.
 * If a valid device was read, *dev_p is non-NULL, otherwise it is NULL
 * (e.g. comment lines, unknown XML content, etc).
 */
static int blkid_parse_line(blkid_cache cache, blkid_dev *dev_p, char *cp)
{
	blkid_dev dev;
	int ret;

	if (!cache || !dev_p)
		return -BLKID_ERR_PARAM;

	*dev_p = NULL;

	DBG(DEBUG_READ, printf("line: %s\n", cp));

	if ((ret = parse_dev(cache, dev_p, &cp)) <= 0)
		return ret;

	dev = *dev_p;

	while ((ret = parse_tag(cache, dev, &cp)) > 0) {
		;
	}

	if (dev->bid_type == NULL) {
		DBG(DEBUG_READ,
		    printf("blkid: device %s has no TYPE\n",dev->bid_name));
		blkid_free_dev(dev);
	}

	DBG(DEBUG_READ, blkid_debug_dump_dev(dev));

	return ret;
}

/*
 * Parse the specified filename, and return the data in the supplied or
 * a newly allocated cache struct.  If the file doesn't exist, return a
 * new empty cache struct.
 */
void blkid_read_cache(blkid_cache cache)
{
	FILE *file;
	char buf[4096];
	int fd, lineno = 0;
	struct stat st;

	if (!cache)
		return;

	/*
	 * If the file doesn't exist, then we just return an empty
	 * struct so that the cache can be populated.
	 */
	if ((fd = open(cache->bic_filename, O_RDONLY)) < 0)
		return;
	if (fstat(fd, &st) < 0)
		goto errout;
	if ((st.st_mtime == cache->bic_ftime) ||
	    (cache->bic_flags & BLKID_BIC_FL_CHANGED)) {
		DBG(DEBUG_CACHE, printf("skipping re-read of %s\n",
					cache->bic_filename));
		goto errout;
	}

	DBG(DEBUG_CACHE, printf("reading cache file %s\n",
				cache->bic_filename));

	file = fdopen(fd, "r");
	if (!file)
		goto errout;

	while (fgets(buf, sizeof(buf), file)) {
		blkid_dev dev;
		unsigned int end;

		lineno++;
		if (buf[0] == 0)
			continue;
		end = strlen(buf) - 1;
		/* Continue reading next line if it ends with a backslash */
		while (buf[end] == '\\' && end < sizeof(buf) - 2 &&
		       fgets(buf + end, sizeof(buf) - end, file)) {
			end = strlen(buf) - 1;
			lineno++;
		}

		if (blkid_parse_line(cache, &dev, buf) < 0) {
			DBG(DEBUG_READ,
			    printf("blkid: bad format on line %d\n", lineno));
			continue;
		}
	}
	fclose(file);

	/*
	 * Initially we do not need to write out the cache file.
	 */
	cache->bic_flags &= ~BLKID_BIC_FL_CHANGED;
	cache->bic_ftime = st.st_mtime;

	return;
errout:
	close(fd);
	return;
}

#ifdef TEST_PROGRAM
static void debug_dump_dev(blkid_dev dev)
{
	struct list_head *p;

	if (!dev) {
		printf("  dev: NULL\n");
		return;
	}

	printf("  dev: name = %s\n", dev->bid_name);
	printf("  dev: DEVNO=\"0x%0llx\"\n", (long long)dev->bid_devno);
	printf("  dev: TIME=\"%lld\"\n", (long long)dev->bid_time);
	printf("  dev: PRI=\"%d\"\n", dev->bid_pri);
	printf("  dev: flags = 0x%08X\n", dev->bid_flags);

	list_for_each(p, &dev->bid_tags) {
		blkid_tag tag = list_entry(p, struct blkid_struct_tag, bit_tags);
		if (tag)
			printf("    tag: %s=\"%s\"\n", tag->bit_name,
			       tag->bit_val);
		else
			printf("    tag: NULL\n");
	}
	printf("\n");
}

int main(int argc, char**argv)
{
	blkid_cache cache = NULL;
	int ret;

	blkid_debug_mask = DEBUG_ALL;
	if (argc > 2) {
		fprintf(stderr, "Usage: %s [filename]\n"
			"Test parsing of the cache (filename)\n", argv[0]);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, argv[1])) < 0)
		fprintf(stderr, "error %d reading cache file %s\n", ret,
			argv[1] ? argv[1] : BLKID_CACHE_FILE);

	blkid_put_cache(cache);

	return ret;
}
#endif
