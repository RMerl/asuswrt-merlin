/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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
#include "lib/torture/torture.h"

static void subunit_suite_start(struct torture_context *ctx,
				struct torture_suite *suite)
{
}

static void subunit_print_testname(struct torture_context *ctx, 
				   struct torture_tcase *tcase,
				   struct torture_test *test)
{
	if (!strcmp(tcase->name, test->name)) {
		printf("%s", test->name);
	} else {
		printf("%s.%s", tcase->name, test->name);
	}
}

static void subunit_test_start(struct torture_context *ctx, 
			       struct torture_tcase *tcase,
			       struct torture_test *test)
{
	printf("test: ");
	subunit_print_testname(ctx, tcase, test);	
	printf("\n");
}

static void subunit_test_result(struct torture_context *context, 
				enum torture_result res, const char *reason)
{
	switch (res) {
	case TORTURE_OK:
		printf("success: ");
		break;
	case TORTURE_FAIL:
		printf("failure: ");
		break;
	case TORTURE_ERROR:
		printf("error: ");
		break;
	case TORTURE_SKIP:
		printf("skip: ");
		break;
	}
	subunit_print_testname(context, context->active_tcase, context->active_test);	

	if (reason)
		printf(" [\n%s\n]", reason);
	printf("\n");
}

static void subunit_comment(struct torture_context *test,
			    const char *comment)
{
	fprintf(stderr, "%s", comment);
}

static void subunit_warning(struct torture_context *test,
			    const char *comment)
{
	fprintf(stderr, "WARNING!: %s\n", comment);
}

const struct torture_ui_ops torture_subunit_ui_ops = {
	.comment = subunit_comment,
	.warning = subunit_warning,
	.test_start = subunit_test_start,
	.test_result = subunit_test_result,
	.suite_start = subunit_suite_start
};
