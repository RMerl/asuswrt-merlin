/* 
   Unix SMB/CIFS implementation.

   util_strlist testing

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009
   
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
#include "torture/torture.h"
#include "param/param.h"

struct test_list_element {
	const char *list_as_string;
	const char *separators;
	const char *list[5];
};

const struct test_list_element test_lists_strings[] = {
	{
		.list_as_string = "",
		.list = { NULL }
	},
	{
		.list_as_string = "foo",
		.list = { "foo", NULL }
	},
	{
		.list_as_string = "foo bar",
		.list = { "foo", "bar", NULL }
	},
	{
		.list_as_string = "foo bar",
		.list = { "foo bar", NULL },
		.separators = ";"
	},
	{
		.list_as_string = "\"foo bar\"",
		.list = { "\"foo", "bar\"", NULL }
	},
	{
		.list_as_string = "\"foo bar\",comma\ttab",
		.list = { "\"foo", "bar\"", "comma", "tab", NULL }
	},
	{
		.list_as_string = "\"foo bar\",comma;semicolon",
		.list = { "\"foo bar\",comma", "semicolon", NULL },
		.separators = ";"
	}
};

const struct test_list_element test_lists_shell_strings[] = {
	{
		.list_as_string = "",
		.list = { NULL }
	},
	{
		.list_as_string = "foo",
		.list = { "foo", NULL }
	},
	{
		.list_as_string = "foo bar",
		.list = { "foo", "bar", NULL }
	},
	{
		.list_as_string = "foo bar",
		.list = { "foo bar", NULL },
		.separators = ";"
	},
	{
		.list_as_string = "\"foo bar\"",
		.list = { "foo bar", NULL }
	},
	{
		.list_as_string = "foo bar \"bla \"",
		.list = { "foo", "bar", "bla ", NULL }
	},
	{
		.list_as_string = "foo \"\" bla",
		.list = { "foo", "", "bla", NULL },
	},
	{
		.list_as_string = "bla \"\"\"\" blie",
		.list = { "bla", "", "", "blie", NULL },
	}
};

static bool test_lists_shell(struct torture_context *tctx, const void *data)
{
	const struct test_list_element *element = data;

	char **ret1, **ret2, *tmp;
	bool match = true;
	TALLOC_CTX *mem_ctx = tctx;

	ret1 = str_list_make_shell(mem_ctx, element->list_as_string, element->separators);
	
	torture_assert(tctx, ret1, "str_list_make_shell() must not return NULL");
	tmp = str_list_join_shell(mem_ctx, (const char **) ret1, element->separators ? *element->separators : ' ');
	ret2 = str_list_make_shell(mem_ctx, tmp, element->separators);

	if ((ret1 == NULL || ret2 == NULL) && ret2 != ret1) {
		match = false;
	} else {
		int j;
		for (j = 0; ret1[j] && ret2[j]; j++) {
			if (strcmp(ret1[j], ret2[j]) != 0) {
				match = false;
				break;
			}
		}

		if (ret1[j] || ret2[j])
			match = false;
	}

	torture_assert(tctx, match, talloc_asprintf(tctx, 
		"str_list_{make,join}_shell: Error double parsing, first run:\n%s\nSecond run: \n%s", element->list_as_string, tmp));
	torture_assert(tctx, str_list_equal((const char * const *) ret1,
					    element->list),
		       talloc_asprintf(tctx, 
				       "str_list_make_shell(%s) failed to create correct list", 
				       element->list_as_string));

	return true;
}

static bool test_list_make(struct torture_context *tctx, const void *data)
{
	const struct test_list_element *element = data;

	char **result;
	result = str_list_make(tctx, element->list_as_string, element->separators);
	torture_assert(tctx, result, "str_list_make() must not return NULL");
	torture_assert(tctx, str_list_equal((const char * const *) result,
					    element->list),
		       talloc_asprintf(tctx, 
				       "str_list_make(%s) failed to create correct list", 
				       element->list_as_string));
	return true;
}

static bool test_list_copy(struct torture_context *tctx)
{
	const char **result;
	const char *list[] = { "foo", "bar", NULL };
	const char *empty_list[] = { NULL };
	const char **null_list = NULL;

	result = (const char **)str_list_copy(tctx, list);
	torture_assert_int_equal(tctx, str_list_length(result), 2, "list length");
	torture_assert_str_equal(tctx, result[0], "foo", "element 0");
	torture_assert_str_equal(tctx, result[1], "bar", "element 1");
	torture_assert_str_equal(tctx, result[2], NULL, "element 2");

	result = (const char **)str_list_copy(tctx, empty_list);
	torture_assert_int_equal(tctx, str_list_length(result), 0, "list length");
	torture_assert_str_equal(tctx, result[0], NULL, "element 0");

	result = (const char **)str_list_copy(tctx, null_list);
	torture_assert(tctx, result == NULL, "result NULL");
	
	return true;
}

static bool test_list_make_empty(struct torture_context *tctx)
{
	char **result;

	result = str_list_make_empty(tctx);
	torture_assert(tctx, result, "str_list_make_empty() must not return NULL");
	torture_assert(tctx, result[0] == NULL, "first element in str_list_make_empty() result must be NULL");

	result = str_list_make(tctx, NULL, NULL);
	torture_assert(tctx, result, "str_list_make() must not return NULL");
	torture_assert(tctx, result[0] == NULL, "first element in str_list_make(ctx, NULL, NULL) result must be NULL");
	
	result = str_list_make(tctx, "", NULL);
	torture_assert(tctx, result, "str_list_make() must not return NULL");
	torture_assert(tctx, result[0] == NULL, "first element in str_list_make(ctx, "", NULL) result must be NULL");
	
	return true;
}

static bool test_list_make_single(struct torture_context *tctx)
{
	char **result;

	result = str_list_make_single(tctx, "foo");

	torture_assert(tctx, result, "str_list_make_single() must not return NULL");
	torture_assert_str_equal(tctx, result[0], "foo", "element 0");
	torture_assert(tctx, result[1] == NULL, "second element in result must be NULL");
	
	return true;
}

static bool test_list_copy_const(struct torture_context *tctx)
{
	const char **result;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_3",
		NULL
	};
	result = str_list_copy_const(tctx, list);
	torture_assert(tctx, result, "str_list_copy() must not return NULL");
	torture_assert(tctx, str_list_equal(result, list), 
		       "str_list_copy() failed");

	return true;
}

static bool test_list_length(struct torture_context *tctx)
{
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_3",
		NULL
	};
	const char *list2[] = {
		NULL
	};
	torture_assert_int_equal(tctx, str_list_length(list), 4, 
		       "str_list_length() failed");

	torture_assert_int_equal(tctx, str_list_length(list2), 0, 
		       "str_list_length() failed");

	torture_assert_int_equal(tctx, str_list_length(NULL), 0, 
		       "str_list_length() failed");

	return true;
}

static bool test_list_add(struct torture_context *tctx)
{
	const char **result, **result2;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_3",
		NULL
	};
	result = (const char **) str_list_make(tctx, "element_0, element_1, element_2", NULL);
	torture_assert(tctx, result, "str_list_make() must not return NULL");
	result2 = str_list_add((const char **) result, "element_3");
	torture_assert(tctx, result2, "str_list_add() must not return NULL");
	torture_assert(tctx, str_list_equal(result2, list), 
		       "str_list_add() failed");

	return true;
}

static bool test_list_add_const(struct torture_context *tctx)
{
	const char **result, **result2;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_3",
		NULL
	};
	result = (const char **) str_list_make(tctx, "element_0, element_1, element_2", NULL);
	torture_assert(tctx, result, "str_list_make() must not return NULL");
	result2 = str_list_add_const(result, "element_3");
	torture_assert(tctx, result2, "str_list_add_const() must not return NULL");
	torture_assert(tctx, str_list_equal(result2, list), 
		       "str_list_add() failed");

	return true;
}

static bool test_list_remove(struct torture_context *tctx)
{
	const char **result;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_3",
		NULL
	};
	result = (const char **) str_list_make(tctx, "element_0, element_1, element_2, element_3", NULL);
	torture_assert(tctx, result, "str_list_make() must not return NULL");
	str_list_remove(result, "element_2");
	torture_assert(tctx, str_list_equal(result, list), 
		       "str_list_remove() failed");

	return true;
}

static bool test_list_check(struct torture_context *tctx)
{
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		NULL
	};
	torture_assert(tctx, str_list_check(list, "element_1"), 
		       "str_list_check() failed");

	return true;
}

static bool test_list_check_ci(struct torture_context *tctx)
{
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		NULL
	};
	torture_assert(tctx, str_list_check_ci(list, "ELEMENT_1"), 
		       "str_list_check_ci() failed");

	return true;
}

static bool test_list_unique(struct torture_context *tctx)
{
	const char **result;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		NULL
	};
	const char *list_dup[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_0", 
		"element_2",
		"element_1",
		"element_1",
		"element_2",
		NULL
	};
	result = (const char **) str_list_copy(tctx, list_dup);
	/* We must copy the list, as str_list_unique does a talloc_realloc() on it's parameter */
	result = str_list_unique(result);
	torture_assert(tctx, result, "str_list_unique() must not return NULL");
	
	torture_assert(tctx, str_list_equal(list, result), 
		       "str_list_unique() failed");

	return true;
}

static bool test_list_unique_2(struct torture_context *tctx)
{
	int i;
	int count, num_dups;
	const char **result;
	const char **list = (const char **)str_list_make_empty(tctx);
	const char **list_dup = (const char **)str_list_make_empty(tctx);

	count = lpcfg_parm_int(tctx->lp_ctx, NULL, "list_unique", "count", 9);
	num_dups = lpcfg_parm_int(tctx->lp_ctx, NULL, "list_unique", "dups", 7);
	torture_comment(tctx, "test_list_unique_2() with %d elements and %d dups\n", count, num_dups);

	for (i = 0; i < count; i++) {
		list = str_list_add_const(list, (const char *)talloc_asprintf(tctx, "element_%03d", i));
	}

	for (i = 0; i < num_dups; i++) {
		list_dup = str_list_append(list_dup, list);
	}

	result = (const char **)str_list_copy(tctx, list_dup);
	/* We must copy the list, as str_list_unique does a talloc_realloc() on it's parameter */
	result = str_list_unique(result);
	torture_assert(tctx, result, "str_list_unique() must not return NULL");

	torture_assert(tctx, str_list_equal(list, result),
		       "str_list_unique() failed");

	return true;
}

static bool test_list_append(struct torture_context *tctx)
{
	const char **result;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		NULL
	};
	const char *list2[] = {
		"element_3", 
		"element_4",
		"element_5",
		NULL
	};
	const char *list_combined[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_3", 
		"element_4",
		"element_5",
		NULL
	};
	result = (const char **) str_list_copy(tctx, list);
	torture_assert(tctx, result, "str_list_copy() must not return NULL");
	result = str_list_append(result, list2);
	torture_assert(tctx, result, "str_list_append() must not return NULL");
	torture_assert(tctx, str_list_equal(list_combined, result), 
		       "str_list_unique() failed");

	return true;
}

static bool test_list_append_const(struct torture_context *tctx)
{
	const char **result;
	const char *list[] = {
		"element_0", 
		"element_1",
		"element_2",
		NULL
	};
	const char *list2[] = {
		"element_3", 
		"element_4",
		"element_5",
		NULL
	};
	const char *list_combined[] = {
		"element_0", 
		"element_1",
		"element_2",
		"element_3", 
		"element_4",
		"element_5",
		NULL
	};
	result = (const char **) str_list_copy(tctx, list);
	torture_assert(tctx, result, "str_list_copy() must not return NULL");
	result = str_list_append_const(result, list2);
	torture_assert(tctx, result, "str_list_append_const() must not return NULL");
	torture_assert(tctx, str_list_equal(list_combined, result), 
		       "str_list_unique() failed");

	return true;
}

struct torture_suite *torture_local_util_strlist(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "strlist");
	int i;

	for (i = 0; i < ARRAY_SIZE(test_lists_shell_strings); i++) {
		char *name;
		name = talloc_asprintf(suite, "lists_shell(%s)",
							   test_lists_shell_strings[i].list_as_string);
		torture_suite_add_simple_tcase_const(suite, name,
			test_lists_shell,  &test_lists_shell_strings[i]);
	}

	for (i = 0; i < ARRAY_SIZE(test_lists_strings); i++) {
		char *name;
		name = talloc_asprintf(suite, "list_make(%s)",
							   test_lists_strings[i].list_as_string);
		torture_suite_add_simple_tcase_const(suite, name,
			test_list_make, &test_lists_strings[i]);
	}

	torture_suite_add_simple_test(suite, "list_copy", test_list_copy);
	torture_suite_add_simple_test(suite, "make_empty", test_list_make_empty);
	torture_suite_add_simple_test(suite, "make_single", test_list_make_single);
	torture_suite_add_simple_test(suite, "list_copy_const", test_list_copy_const);
	torture_suite_add_simple_test(suite, "list_length", test_list_length);
	torture_suite_add_simple_test(suite, "list_add", test_list_add);
	torture_suite_add_simple_test(suite, "list_add_const", test_list_add_const);
	torture_suite_add_simple_test(suite, "list_remove", test_list_remove);
	torture_suite_add_simple_test(suite, "list_check", test_list_check);
	torture_suite_add_simple_test(suite, "list_check_ci", test_list_check_ci);
	torture_suite_add_simple_test(suite, "list_unique", test_list_unique);
	torture_suite_add_simple_test(suite, "list_unique_2", test_list_unique_2);
	torture_suite_add_simple_test(suite, "list_append", test_list_append);
	torture_suite_add_simple_test(suite, "list_append_const", test_list_append_const);

	return suite;
}
