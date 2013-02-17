/*
 * parse_opt.c -- mount option string parsing helpers
 *
 * Copyright (C) 2007 Oracle.  All rights reserved.
 * Copyright (C) 2007 Chuck Lever <chuck.lever@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 */

/*
 * Converting a C string containing mount options to a data object
 * and manipulating that object is cleaner in C than manipulating
 * the C string itself.  This is similar to the way Python handles
 * string manipulation.
 *
 * The current implementation uses a linked list as the data object
 * since lists are simple, and we don't need to worry about more
 * than ten or twenty options at a time.
 *
 * Hopefully the interface is abstract enough that the underlying
 * data structure can be replaced if needed without changing the API.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "parse_opt.h"
#include "token.h"


struct mount_option {
	struct mount_option *next, *prev;
	char *keyword;
	char *value;
};

struct mount_options {
	struct mount_option *head, *tail;
	unsigned int count;
};

static struct mount_option *option_create(char *str)
{
	struct mount_option *option;
	char *opteq;

	if (!str)
		return NULL;

	option = malloc(sizeof(*option));
	if (!option)
		return NULL;
	
	option->next = NULL;
	option->prev = NULL;

	opteq = strchr(str, '=');
	if (opteq) {
		option->keyword = strndup(str, opteq - str);
		if (!option->keyword)
			goto fail;
		option->value = strdup(opteq + 1);
		if (!option->value) {
			free(option->keyword);
			goto fail;
		}
	} else {
		option->keyword = strdup(str);
		if (!option->keyword)
			goto fail;
		option->value = NULL;
	}

	return option;

fail:
	free(option);
	return NULL;
}

static void option_destroy(struct mount_option *option)
{
	free(option->keyword);
	free(option->value);
	free(option);
}

static void options_init(struct mount_options *options)
{
	options->head = options->tail = NULL;
	options->count = 0;
}

static struct mount_options *options_create(void)
{
	struct mount_options *options;

	options = malloc(sizeof(*options));
	if (options)
		options_init(options);

	return options;
}

static int options_empty(struct mount_options *options)
{
	return options->count == 0;
}

static void options_tail_insert(struct mount_options *options,
				struct mount_option *option)
{
	struct mount_option *prev = options->tail;

	option->next = NULL;
	option->prev = prev;

	if (prev)
		prev->next = option;
	else
		options->head = option;
	options->tail = option;

	options->count++;
}

static void options_delete(struct mount_options *options,
			   struct mount_option *option)
{
	struct mount_option *prev = option->prev;
	struct mount_option *next = option->next;

	if (!options_empty(options)) {
		if (prev)
			prev->next = option->next;
		if (next)
			next->prev = option->prev;

		if (options->head == option)
			options->head = option->next;
		if (options->tail == option)
			options->tail = prev;

		options->count--;

		option_destroy(option);
	}
}


/**
 * po_destroy - deallocate a group of mount options
 * @options: pointer to mount options to free
 *
 */
void po_destroy(struct mount_options *options)
{
	if (options) {
		while (!options_empty(options))
			options_delete(options, options->head);
		free(options);
	}
}

/**
 * po_split - split options string into group of options
 * @options: pointer to C string containing zero or more comma-delimited options
 *
 * Convert our mount options string to a list to make it easier
 * to adjust the options as we go.  This is just an exercise in
 * lexical parsing -- this function doesn't pay attention to the
 * meaning of the options themselves.
 *
 * Returns a new group of mount options if successful; otherwise NULL
 * is returned if some failure occurred.
 */
struct mount_options *po_split(char *str)
{
	struct mount_options *options;
	struct tokenizer_state *tstate;
	char *opt;

	if (!str)
		return options_create();

	options = options_create();
	if (options) {
		tstate = init_tokenizer(str, ',');
		for (opt = next_token(tstate); opt; opt = next_token(tstate)) {
			struct mount_option *option = option_create(opt);
			free(opt);
			if (!option)
				goto fail;
			options_tail_insert(options, option);
		}
		if (tokenizer_error(tstate))
			goto fail;
		end_tokenizer(tstate);
	}
	return options;

fail:
	end_tokenizer(tstate);
	po_destroy(options);
	return NULL;
}

/**
 * po_replace - replace mount options in one mount_options object with another
 * @target: pointer to previously instantiated object to replace
 * @source: pointer to object containing source mount options
 *
 * Side effect: the object referred to by source is emptied.
 */
void po_replace(struct mount_options *target, struct mount_options *source)
{
	if (target) {
		while (!options_empty(target))
			options_delete(target, target->head);

		if (source) {
			target->head = source->head;
			target->tail = source->tail;
			target->count = source->count;

			options_init(source);
		}
	}
}

/**
 * po_join - recombine group of mount options into a C string
 * @options: pointer to mount options to recombine
 * @str: handle on string to replace (input and output)
 *
 * Convert our mount options object back into a string that the
 * rest of the world can use.
 *
 * Upon return, @string contains the address of a replacement
 * C string containing a comma-delimited list of mount options
 * and values; or the passed-in string is freed and NULL is
 * returned if some failure occurred.
 */
po_return_t po_join(struct mount_options *options, char **str)
{
	size_t len = 0;
	struct mount_option *option;

	if (!str || !options)
		return PO_FAILED;
		
	free(*str);
	*str = NULL;

	if (options_empty(options)) {
		*str = strdup("");
		return *str ? PO_SUCCEEDED : PO_FAILED;
	}

	for (option = options->head; option; option = option->next) {
		len += strlen(option->keyword);
		if (option->value)
			len +=strlen(option->value) + 1;  /* equals sign */
		if (option->next)
			len++;  /* comma */
	}

	len++;	/* NULL on the end */

	*str = malloc(len);
	if (!*str)
		return PO_FAILED;
	*str[0] = '\0';

	for (option = options->head; option; option = option->next) {
		strcat(*str, option->keyword);
		if (option->value) {
			strcat(*str, "=");
			strcat(*str, option->value);
		}
		if (option->next)
			strcat(*str, ",");
	}

	return PO_SUCCEEDED;
}

/**
 * po_append - concatenate an option onto a group of options
 * @options: pointer to mount options
 * @option: pointer to a C string containing the option to add
 *
 */
po_return_t po_append(struct mount_options *options, char *str)
{
	struct mount_option *option = option_create(str);

	if (option) {
		options_tail_insert(options, option);
		return PO_SUCCEEDED;
	}
	return PO_FAILED;
}

/**
 * po_contains - check for presense of an option in a group
 * @options: pointer to mount options
 * @keyword: pointer to a C string containing option keyword for which to search
 *
 */
po_found_t po_contains(struct mount_options *options, char *keyword)
{
	struct mount_option *option;

	if (options && keyword) {
		for (option = options->head; option; option = option->next)
			if (strcmp(option->keyword, keyword) == 0)
				return PO_FOUND;
	}

	return PO_NOT_FOUND;
}

/**
 * po_get - return the value of the rightmost instance of an option
 * @options: pointer to mount options
 * @keyword: pointer to a C string containing option keyword for which to search
 *
 * If multiple instances of the same option are present in a mount option
 * list, the rightmost instance is always the effective one.
 *
 * Returns pointer to C string containing the value of the option.
 * Returns NULL if the option isn't found, or if the option doesn't
 * have a value.
 */
char *po_get(struct mount_options *options, char *keyword)
{
	struct mount_option *option;

	if (options && keyword) {
		for (option = options->tail; option; option = option->prev)
			if (strcmp(option->keyword, keyword) == 0)
				return option->value;
	}

	return NULL;
}

/**
 * po_get_numeric - return numeric value of rightmost instance of keyword option
 * @options: pointer to mount options
 * @keyword: pointer to a C string containing option keyword for which to search
 * @value: OUT: set to the value of the keyword
 *
 * This is specifically for parsing keyword options that take only a numeric
 * value.  If multiple instances of the same option are present in a mount
 * option list, the rightmost instance is always the effective one.
 *
 * Returns:
 *	* PO_FOUND if the keyword was found and the value is numeric; @value is
 *	  set to the keyword's value
 *	* PO_NOT_FOUND if the keyword was not found
 *	* PO_BAD_VALUE if the keyword was found, but the value is not numeric
 *
 * These last two are separate in case the caller wants to warn about bad mount
 * options instead of silently using a default.
 */
#ifdef HAVE_STRTOL
po_found_t po_get_numeric(struct mount_options *options, char *keyword, long *value)
{
	char *option, *endptr;
	long tmp;

	option = po_get(options, keyword);
	if (option == NULL)
		return PO_NOT_FOUND;

	errno = 0;
	tmp = strtol(option, &endptr, 10);
	if (errno == 0 && endptr != option) {
		*value = tmp;
		return PO_FOUND;
	}
	return PO_BAD_VALUE;
}
#else	/* HAVE_STRTOL */
po_found_t po_get_numeric(struct mount_options *options, char *keyword, long *value)
{
	char *option;

	option = po_get(options, keyword);
	if (option == NULL)
		return PO_NOT_FOUND;

	*value = atoi(option);
	return PO_FOUND;
}
#endif	/* HAVE_STRTOL */

/**
 * po_rightmost - determine the relative position of several options
 * @options: pointer to mount options
 * @keys: pointer to an array of C strings containing option keywords
 *
 * This function can be used to determine which of several similar
 * options will be the one to take effect.
 *
 * The kernel parses the mount option string from left to right.
 * If an option is specified more than once (for example, "intr"
 * and "nointr", the rightmost option is the last to be parsed,
 * and it therefore takes precedence over previous similar options.
 *
 * This can also distinguish among multiple synonymous options, such
 * as "proto=," "udp" and "tcp."
 *
 * Returns the index into @keys of the option that is rightmost.
 * If none of the options listed in @keys is present in @options, or
 * if @options is NULL, returns -1.
 */
int po_rightmost(struct mount_options *options, const char *keys[])
{
	struct mount_option *option;
	unsigned int i;

	if (options) {
		for (option = options->tail; option; option = option->prev) {
			for (i = 0; keys[i] != NULL; i++)
				if (strcmp(option->keyword, keys[i]) == 0)
					return i;
		}
	}

	return -1;
}

/**
 * po_remove_all - remove instances of an option from a group
 * @options: pointer to mount options
 * @keyword: pointer to a C string containing an option keyword to remove
 *
 * Side-effect: the passed-in list is truncated on success.
 */
po_found_t po_remove_all(struct mount_options *options, char *keyword)
{
	struct mount_option *option, *next;
	int found = PO_NOT_FOUND;

	if (options && keyword) {
		for (option = options->head; option; option = next) {
			next = option->next;
			if (strcmp(option->keyword, keyword) == 0) {
				options_delete(options, option);
				found = PO_FOUND;
			}
		}
	}

	return found;
}
