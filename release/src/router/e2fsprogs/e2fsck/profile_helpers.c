/*
 * profile_helpers.c -- Helper functions for the profile library
 *
 * These functions are not part of the "core" profile library, and do
 * not require access to the internal functions and data structures of
 * the profile library.  They are mainly convenience functions for
 * programs that want to do something unusual such as obtaining the
 * list of sections or relations, or accessing multiple values from a
 * relation that is listed more than once.  This functionality can all
 * be done using the profile_iterator abstraction, but it is less
 * convenient.
 *
 * Copyright (C) 2006 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <et/com_err.h>
#include "profile.h"
#include "prof_err.h"

/*
 * These functions --- init_list(), end_list(), and add_to_list() are
 * internal functions used to build up a null-terminated char ** list
 * of strings to be returned by functions like profile_get_values.
 *
 * The profile_string_list structure is used for internal booking
 * purposes to build up the list, which is returned in *ret_list by
 * the end_list() function.
 *
 * The publicly exported interface for freeing char** list is
 * profile_free_list().
 */

struct profile_string_list {
	char	**list;
	int	num;
	int	max;
};

/*
 * Initialize the string list abstraction.
 */
static errcode_t init_list(struct profile_string_list *list)
{
	list->num = 0;
	list->max = 10;
	list->list = malloc(list->max * sizeof(char *));
	if (list->list == 0)
		return ENOMEM;
	list->list[0] = 0;
	return 0;
}

/*
 * Free any memory left over in the string abstraction, returning the
 * built up list in *ret_list if it is non-null.
 */
static void end_list(struct profile_string_list *list, char ***ret_list)
{
	char	**cp;

	if (list == 0)
		return;

	if (ret_list) {
		*ret_list = list->list;
		return;
	} else {
		for (cp = list->list; *cp; cp++)
			free(*cp);
		free(list->list);
	}
	list->num = list->max = 0;
	list->list = 0;
}

/*
 * Add a string to the list.
 */
static errcode_t add_to_list(struct profile_string_list *list, char *str)
{
	char 	**newlist;
	int	newmax;

	if (list->num+1 >= list->max) {
		newmax = list->max + 10;
		newlist = realloc(list->list, newmax * sizeof(char *));
		if (newlist == 0)
			return ENOMEM;
		list->max = newmax;
		list->list = newlist;
	}

	list->list[list->num++] = str;
	list->list[list->num] = 0;
	return 0;
}

/*
 * Return TRUE if the string is already a member of the list.
 */
static int is_list_member(struct profile_string_list *list, const char *str)
{
	char **cpp;

	if (!list->list)
		return 0;

	for (cpp = list->list; *cpp; cpp++) {
		if (!strcmp(*cpp, str))
			return 1;
	}
	return 0;
}

/*
 * This function frees a null-terminated list as returned by
 * profile_get_values.
 */
void profile_free_list(char **list)
{
    char	**cp;

    if (list == 0)
	    return;

    for (cp = list; *cp; cp++)
	free(*cp);
    free(list);
}

errcode_t
profile_get_values(profile_t profile, const char *const *names,
		   char ***ret_values)
{
	errcode_t		retval;
	void			*state;
	char			*value;
	struct profile_string_list values;

	if ((retval = profile_iterator_create(profile, names,
					      PROFILE_ITER_RELATIONS_ONLY,
					      &state)))
		return retval;

	if ((retval = init_list(&values)))
		return retval;

	do {
		if ((retval = profile_iterator(&state, 0, &value)))
			goto cleanup;
		if (value)
			add_to_list(&values, value);
	} while (state);

	if (values.num == 0) {
		retval = PROF_NO_RELATION;
		goto cleanup;
	}

	end_list(&values, ret_values);
	return 0;

cleanup:
	end_list(&values, 0);
	return retval;
}

/*
 * This function will return the list of the names of subections in the
 * under the specified section name.
 */
errcode_t
profile_get_subsection_names(profile_t profile, const char **names,
			     char ***ret_names)
{
	errcode_t		retval;
	void			*state;
	char			*name;
	struct profile_string_list values;

	if ((retval = profile_iterator_create(profile, names,
		   PROFILE_ITER_LIST_SECTION | PROFILE_ITER_SECTIONS_ONLY,
		   &state)))
		return retval;

	if ((retval = init_list(&values)))
		return retval;

	do {
		if ((retval = profile_iterator(&state, &name, 0)))
			goto cleanup;
		if (name)
			add_to_list(&values, name);
	} while (state);

	end_list(&values, ret_names);
	return 0;

cleanup:
	end_list(&values, 0);
	return retval;
}

/*
 * This function will return the list of the names of relations in the
 * under the specified section name.
 */
errcode_t
profile_get_relation_names(profile_t profile, const char **names,
			   char ***ret_names)
{
	errcode_t		retval;
	void			*state;
	char			*name;
	struct profile_string_list values;

	if ((retval = profile_iterator_create(profile, names,
		   PROFILE_ITER_LIST_SECTION | PROFILE_ITER_RELATIONS_ONLY,
		   &state)))
		return retval;

	if ((retval = init_list(&values)))
		return retval;

	do {
		if ((retval = profile_iterator(&state, &name, 0)))
			goto cleanup;
		if (name) {
			if (is_list_member(&values, name))
				free(name);
			else
				add_to_list(&values, name);
		}
	} while (state);

	end_list(&values, ret_names);
	return 0;

cleanup:
	end_list(&values, 0);
	return retval;
}


void
profile_release_string(char *str)
{
	free(str);
}

errcode_t
profile_init_path(const char * filepath,
		  profile_t *ret_profile)
{
	int n_entries, i;
	unsigned int ent_len;
	const char *s, *t;
	char **filenames;
	errcode_t retval;

	/* count the distinct filename components */
	for(s = filepath, n_entries = 1; *s; s++) {
		if (*s == ':')
			n_entries++;
	}

	/* the array is NULL terminated */
	filenames = (char **) malloc((n_entries+1) * sizeof(char*));
	if (filenames == 0)
		return ENOMEM;

	/* measure, copy, and skip each one */
	for(s = filepath, i=0; (t = strchr(s, ':')) || (t=s+strlen(s)); s=t+1, i++) {
		ent_len = t-s;
		filenames[i] = (char*) malloc(ent_len + 1);
		if (filenames[i] == 0) {
			/* if malloc fails, free the ones that worked */
			while(--i >= 0) free(filenames[i]);
                        free(filenames);
			return ENOMEM;
		}
		strncpy(filenames[i], s, ent_len);
		filenames[i][ent_len] = 0;
		if (*t == 0) {
			i++;
			break;
		}
	}
	/* cap the array */
	filenames[i] = 0;

	retval = profile_init((const char **) filenames,
			      ret_profile);

	/* count back down and free the entries */
	while(--i >= 0) free(filenames[i]);
	free(filenames);

	return retval;
}
