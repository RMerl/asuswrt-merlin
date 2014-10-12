/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/locale.h"
#include "lib/util/tsort.h"

#undef strcasecmp

/**
 * @file
 * @brief String list manipulation
 */

/**
  build an empty (only NULL terminated) list of strings (for expansion with str_list_add() etc)
*/
_PUBLIC_ char **str_list_make_empty(TALLOC_CTX *mem_ctx)
{
	char **ret = NULL;

	ret = talloc_array(mem_ctx, char *, 1);
	if (ret == NULL) {
		return NULL;
	}

	ret[0] = NULL;

	return ret;
}

/**
  place the only element 'entry' into a new, NULL terminated string list
*/
_PUBLIC_ char **str_list_make_single(TALLOC_CTX *mem_ctx, const char *entry)
{
	char **ret = NULL;

	ret = talloc_array(mem_ctx, char *, 2);
	if (ret == NULL) {
		return NULL;
	}

	ret[0] = talloc_strdup(ret, entry);
	if (!ret[0]) {
		talloc_free(ret);
		return NULL;
	}
	ret[1] = NULL;

	return ret;
}

/**
  build a null terminated list of strings from a input string and a
  separator list. The separator list must contain characters less than
  or equal to 0x2f for this to work correctly on multi-byte strings
*/
_PUBLIC_ char **str_list_make(TALLOC_CTX *mem_ctx, const char *string, const char *sep)
{
	int num_elements = 0;
	char **ret = NULL;

	if (sep == NULL) {
		sep = LIST_SEP;
	}

	ret = talloc_array(mem_ctx, char *, 1);
	if (ret == NULL) {
		return NULL;
	}

	while (string && *string) {
		size_t len = strcspn(string, sep);
		char **ret2;
		
		if (len == 0) {
			string += strspn(string, sep);
			continue;
		}

		ret2 = talloc_realloc(mem_ctx, ret, char *,
			num_elements+2);
		if (ret2 == NULL) {
			talloc_free(ret);
			return NULL;
		}
		ret = ret2;

		ret[num_elements] = talloc_strndup(ret, string, len);
		if (ret[num_elements] == NULL) {
			talloc_free(ret);
			return NULL;
		}

		num_elements++;
		string += len;
	}

	ret[num_elements] = NULL;

	return ret;
}

/**
 * build a null terminated list of strings from an argv-like input string 
 * Entries are separated by spaces and can be enclosed by quotes.
 * Does NOT support escaping
 */
_PUBLIC_ char **str_list_make_shell(TALLOC_CTX *mem_ctx, const char *string, const char *sep)
{
	int num_elements = 0;
	char **ret = NULL;

	ret = talloc_array(mem_ctx, char *, 1);
	if (ret == NULL) {
		return NULL;
	}

	if (sep == NULL)
		sep = " \t\n\r";

	while (string && *string) {
		size_t len = strcspn(string, sep);
		char *element;
		char **ret2;
		
		if (len == 0) {
			string += strspn(string, sep);
			continue;
		}

		if (*string == '\"') {
			string++;
			len = strcspn(string, "\"");
			element = talloc_strndup(ret, string, len);
			string += len + 1;
		} else {
			element = talloc_strndup(ret, string, len);
			string += len;
		}

		if (element == NULL) {
			talloc_free(ret);
			return NULL;
		}

		ret2 = talloc_realloc(mem_ctx, ret, char *, num_elements+2);
		if (ret2 == NULL) {
			talloc_free(ret);
			return NULL;
		}
		ret = ret2;

		ret[num_elements] = element;	

		num_elements++;
	}

	ret[num_elements] = NULL;

	return ret;

}

/**
 * join a list back to one string 
 */
_PUBLIC_ char *str_list_join(TALLOC_CTX *mem_ctx, const char **list, char separator)
{
	char *ret = NULL;
	int i;
	
	if (list[0] == NULL)
		return talloc_strdup(mem_ctx, "");

	ret = talloc_strdup(mem_ctx, list[0]);

	for (i = 1; list[i]; i++) {
		ret = talloc_asprintf_append_buffer(ret, "%c%s", separator, list[i]);
	}

	return ret;
}

/** join a list back to one (shell-like) string; entries 
 * separated by spaces, using quotes where necessary */
_PUBLIC_ char *str_list_join_shell(TALLOC_CTX *mem_ctx, const char **list, char sep)
{
	char *ret = NULL;
	int i;
	
	if (list[0] == NULL)
		return talloc_strdup(mem_ctx, "");

	if (strchr(list[0], ' ') || strlen(list[0]) == 0) 
		ret = talloc_asprintf(mem_ctx, "\"%s\"", list[0]);
	else 
		ret = talloc_strdup(mem_ctx, list[0]);

	for (i = 1; list[i]; i++) {
		if (strchr(list[i], ' ') || strlen(list[i]) == 0) 
			ret = talloc_asprintf_append_buffer(ret, "%c\"%s\"", sep, list[i]);
		else 
			ret = talloc_asprintf_append_buffer(ret, "%c%s", sep, list[i]);
	}

	return ret;
}

/**
  return the number of elements in a string list
*/
_PUBLIC_ size_t str_list_length(const char * const *list)
{
	size_t ret;
	for (ret=0;list && list[ret];ret++) /* noop */ ;
	return ret;
}


/**
  copy a string list
*/
_PUBLIC_ char **str_list_copy(TALLOC_CTX *mem_ctx, const char **list)
{
	int i;
	char **ret;

	if (list == NULL)
		return NULL;
	
	ret = talloc_array(mem_ctx, char *, str_list_length(list)+1);
	if (ret == NULL) 
		return NULL;

	for (i=0;list && list[i];i++) {
		ret[i] = talloc_strdup(ret, list[i]);
		if (ret[i] == NULL) {
			talloc_free(ret);
			return NULL;
		}
	}
	ret[i] = NULL;
	return ret;
}

/**
   Return true if all the elements of the list match exactly.
 */
_PUBLIC_ bool str_list_equal(const char * const *list1,
			     const char * const *list2)
{
	int i;
	
	if (list1 == NULL || list2 == NULL) {
		return (list1 == list2); 
	}
	
	for (i=0;list1[i] && list2[i];i++) {
		if (strcmp(list1[i], list2[i]) != 0) {
			return false;
		}
	}
	if (list1[i] || list2[i]) {
		return false;
	}
	return true;
}


/**
  add an entry to a string list
*/
_PUBLIC_ const char **str_list_add(const char **list, const char *s)
{
	size_t len = str_list_length(list);
	const char **ret;

	ret = talloc_realloc(NULL, list, const char *, len+2);
	if (ret == NULL) return NULL;

	ret[len] = talloc_strdup(ret, s);
	if (ret[len] == NULL) return NULL;

	ret[len+1] = NULL;

	return ret;
}

/**
  remove an entry from a string list
*/
_PUBLIC_ void str_list_remove(const char **list, const char *s)
{
	int i;

	for (i=0;list[i];i++) {
		if (strcmp(list[i], s) == 0) break;
	}
	if (!list[i]) return;

	for (;list[i];i++) {
		list[i] = list[i+1];
	}
}


/**
  return true if a string is in a list
*/
_PUBLIC_ bool str_list_check(const char **list, const char *s)
{
	int i;

	for (i=0;list[i];i++) {
		if (strcmp(list[i], s) == 0) return true;
	}
	return false;
}

/**
  return true if a string is in a list, case insensitively
*/
_PUBLIC_ bool str_list_check_ci(const char **list, const char *s)
{
	int i;

	for (i=0;list[i];i++) {
		if (strcasecmp(list[i], s) == 0) return true;
	}
	return false;
}


/**
  append one list to another - expanding list1
*/
_PUBLIC_ const char **str_list_append(const char **list1,
	const char * const *list2)
{
	size_t len1 = str_list_length(list1);
	size_t len2 = str_list_length(list2);
	const char **ret;
	int i;

	ret = talloc_realloc(NULL, list1, const char *, len1+len2+1);
	if (ret == NULL) return NULL;

	for (i=len1;i<len1+len2;i++) {
		ret[i] = talloc_strdup(ret, list2[i-len1]);
		if (ret[i] == NULL) {
			return NULL;
		}
	}
	ret[i] = NULL;

	return ret;
}

static int list_cmp(const char **el1, const char **el2)
{
	return strcmp(*el1, *el2);
}

/*
  return a list that only contains the unique elements of a list,
  removing any duplicates
 */
_PUBLIC_ const char **str_list_unique(const char **list)
{
	size_t len = str_list_length(list);
	const char **list2;
	int i, j;
	if (len < 2) {
		return list;
	}
	list2 = (const char **)talloc_memdup(list, list,
					     sizeof(list[0])*(len+1));
	TYPESAFE_QSORT(list2, len, list_cmp);
	list[0] = list2[0];
	for (i=j=1;i<len;i++) {
		if (strcmp(list2[i], list[j-1]) != 0) {
			list[j] = list2[i];
			j++;
		}
	}
	list[j] = NULL;
	list = talloc_realloc(NULL, list, const char *, j + 1);
	talloc_free(list2);
	return list;
}

/*
  very useful when debugging complex list related code
 */
_PUBLIC_ void str_list_show(const char **list)
{
	int i;
	DEBUG(0,("{ "));
	for (i=0;list && list[i];i++) {
		DEBUG(0,("\"%s\", ", list[i]));
	}
	DEBUG(0,("}\n"));
}



/**
  append one list to another - expanding list1
  this assumes the elements of list2 are const pointers, so we can re-use them
*/
_PUBLIC_ const char **str_list_append_const(const char **list1,
					    const char **list2)
{
	size_t len1 = str_list_length(list1);
	size_t len2 = str_list_length(list2);
	const char **ret;
	int i;

	ret = talloc_realloc(NULL, list1, const char *, len1+len2+1);
	if (ret == NULL) return NULL;

	for (i=len1;i<len1+len2;i++) {
		ret[i] = list2[i-len1];
	}
	ret[i] = NULL;

	return ret;
}

/**
  add an entry to a string list
  this assumes s will not change
*/
_PUBLIC_ const char **str_list_add_const(const char **list, const char *s)
{
	size_t len = str_list_length(list);
	const char **ret;

	ret = talloc_realloc(NULL, list, const char *, len+2);
	if (ret == NULL) return NULL;

	ret[len] = s;
	ret[len+1] = NULL;

	return ret;
}

/**
  copy a string list
  this assumes list will not change
*/
_PUBLIC_ const char **str_list_copy_const(TALLOC_CTX *mem_ctx,
					  const char **list)
{
	int i;
	const char **ret;

	if (list == NULL)
		return NULL;
	
	ret = talloc_array(mem_ctx, const char *, str_list_length(list)+1);
	if (ret == NULL) 
		return NULL;

	for (i=0;list && list[i];i++) {
		ret[i] = list[i];
	}
	ret[i] = NULL;
	return ret;
}

/**
 * Needed for making an "unconst" list "const"
 */
_PUBLIC_ const char **const_str_list(char **list)
{
	return (const char **)list;
}

