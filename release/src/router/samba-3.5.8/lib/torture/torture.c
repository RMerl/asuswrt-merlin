/* 
   Unix SMB/CIFS implementation.
   SMB torture UI functions

   Copyright (C) Jelmer Vernooij 2006-2008
   
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

#include "source4/include/includes.h"
#include "../torture/torture.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"
#include "system/filesys.h"

struct torture_results *torture_results_init(TALLOC_CTX *mem_ctx, const struct torture_ui_ops *ui_ops)
{
	struct torture_results *results = talloc_zero(mem_ctx, struct torture_results);

	results->ui_ops = ui_ops;
	results->returncode = true;

	if (ui_ops->init)
		ui_ops->init(results);

	return results;
}

/**
 * Initialize a torture context
 */
struct torture_context *torture_context_init(struct tevent_context *event_ctx, 
											 struct torture_results *results)
{
	struct torture_context *torture = talloc_zero(event_ctx, 
						      struct torture_context);

	if (torture == NULL)
		return NULL;

	torture->ev = event_ctx;
	torture->results = talloc_reference(torture, results);

	return torture;
}

/**
 * Create a sub torture context
 */
struct torture_context *torture_context_child(struct torture_context *parent)
{
	struct torture_context *subtorture = talloc_zero(parent, struct torture_context);

	if (subtorture == NULL)
		return NULL;

	subtorture->ev = talloc_reference(subtorture, parent->ev);
	subtorture->lp_ctx = talloc_reference(subtorture, parent->lp_ctx);
	subtorture->outputdir = talloc_reference(subtorture, parent->outputdir);
	subtorture->results = talloc_reference(subtorture, parent->results);

	return subtorture;
}	

/**
 create a temporary directory.
*/
_PUBLIC_ NTSTATUS torture_temp_dir(struct torture_context *tctx, 
				   const char *prefix, 
				   char **tempdir)
{
	SMB_ASSERT(tctx->outputdir != NULL);

	*tempdir = talloc_asprintf(tctx, "%s/%s.XXXXXX", tctx->outputdir, 
				   prefix);
	NT_STATUS_HAVE_NO_MEMORY(*tempdir);

	if (mkdtemp(*tempdir) == NULL) {
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
}

/**
 * Comment on the status/progress of a test
 */
void torture_comment(struct torture_context *context, const char *comment, ...)
{
	va_list ap;
	char *tmp;

	if (!context->results->ui_ops->comment)
		return;

	va_start(ap, comment);
	tmp = talloc_vasprintf(context, comment, ap);
	va_end(ap);
		
	context->results->ui_ops->comment(context, tmp);
	
	talloc_free(tmp);
}

/**
 * Print a warning about the current test
 */
void torture_warning(struct torture_context *context, const char *comment, ...)
{
	va_list ap;
	char *tmp;

	if (!context->results->ui_ops->warning)
		return;

	va_start(ap, comment);
	tmp = talloc_vasprintf(context, comment, ap);
	va_end(ap);

	context->results->ui_ops->warning(context, tmp);

	talloc_free(tmp);
}

/**
 * Store the result of a torture test.
 */
void torture_result(struct torture_context *context, 
		    enum torture_result result, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (context->last_reason) {
		torture_warning(context, "%s", context->last_reason);
		talloc_free(context->last_reason);
	}

	context->last_result = result;
	context->last_reason = talloc_vasprintf(context, fmt, ap);
	va_end(ap);
}

/**
 * Create a new torture suite
 */
struct torture_suite *torture_suite_create(TALLOC_CTX *ctx, const char *name)
{
	struct torture_suite *suite = talloc_zero(ctx, struct torture_suite);

	suite->name = talloc_strdup(suite, name);
	suite->testcases = NULL;
	suite->children = NULL;

	return suite;
}

/**
 * Set the setup() and teardown() functions for a testcase.
 */
void torture_tcase_set_fixture(struct torture_tcase *tcase, 
		bool (*setup) (struct torture_context *, void **),
		bool (*teardown) (struct torture_context *, void *))
{
	tcase->setup = setup;
	tcase->teardown = teardown;
}

static bool wrap_test_with_testcase_const(struct torture_context *torture_ctx,
				    struct torture_tcase *tcase,
				    struct torture_test *test)
{
	bool (*fn) (struct torture_context *,
		    const void *tcase_data,
		    const void *test_data);

	fn = test->fn;

	return fn(torture_ctx, tcase->data, test->data);
}

/**
 * Add a test that uses const data to a testcase
 */
struct torture_test *torture_tcase_add_test_const(struct torture_tcase *tcase,
		const char *name,
		bool (*run) (struct torture_context *, const void *tcase_data,
			const void *test_data),
		const void *data)
{
	struct torture_test *test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_test_with_testcase_const;
	test->fn = run;
	test->dangerous = false;
	test->data = data;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}

/**
 * Add a new testcase
 */
bool torture_suite_init_tcase(struct torture_suite *suite, 
			      struct torture_tcase *tcase, 
			      const char *name)
{
	tcase->name = talloc_strdup(tcase, name);
	tcase->description = NULL;
	tcase->setup = NULL;
	tcase->teardown = NULL;
	tcase->fixture_persistent = true;
	tcase->tests = NULL;

	DLIST_ADD_END(suite->testcases, tcase, struct torture_tcase *);

	return true;
}


struct torture_tcase *torture_suite_add_tcase(struct torture_suite *suite, 
							 const char *name)
{
	struct torture_tcase *tcase = talloc(suite, struct torture_tcase);

	if (!torture_suite_init_tcase(suite, tcase, name))
		return NULL;

	return tcase;
}

/**
 * Run a torture test suite.
 */
bool torture_run_suite(struct torture_context *context, 
		       struct torture_suite *suite)
{
	bool ret = true;
	struct torture_tcase *tcase;
	struct torture_suite *tsuite;
	char *old_testname;

	if (context->results->ui_ops->suite_start)
		context->results->ui_ops->suite_start(context, suite);

	old_testname = context->active_testname;
	if (old_testname != NULL)
		context->active_testname = talloc_asprintf(context, "%s-%s", 
							   old_testname, suite->name);
	else
		context->active_testname = talloc_strdup(context, suite->name);

	for (tcase = suite->testcases; tcase; tcase = tcase->next) {
		ret &= torture_run_tcase(context, tcase);
	}

	for (tsuite = suite->children; tsuite; tsuite = tsuite->next) {
		ret &= torture_run_suite(context, tsuite);
	}

	talloc_free(context->active_testname);
	context->active_testname = old_testname;

	if (context->results->ui_ops->suite_finish)
		context->results->ui_ops->suite_finish(context, suite);

	return ret;
}

void torture_ui_test_start(struct torture_context *context, 
			   struct torture_tcase *tcase, 
			   struct torture_test *test)
{
	if (context->results->ui_ops->test_start)
		context->results->ui_ops->test_start(context, tcase, test);
}

void torture_ui_test_result(struct torture_context *context, 
			    enum torture_result result,
			    const char *comment)
{
	if (context->results->ui_ops->test_result)
		context->results->ui_ops->test_result(context, result, comment);

	if (result == TORTURE_ERROR || result == TORTURE_FAIL)
		context->results->returncode = false;
}

static bool internal_torture_run_test(struct torture_context *context, 
					  struct torture_tcase *tcase,
					  struct torture_test *test,
					  bool already_setup)
{
	bool success;
	char *old_testname = NULL;

	if (tcase == NULL || strcmp(test->name, tcase->name) != 0) { 
		old_testname = context->active_testname;
		context->active_testname = talloc_asprintf(context, "%s-%s", old_testname, test->name);
	}

	context->active_tcase = tcase;
	context->active_test = test;

	torture_ui_test_start(context, tcase, test);

	context->last_reason = NULL;
	context->last_result = TORTURE_OK;

	if (!already_setup && tcase->setup && 
		!tcase->setup(context, &(tcase->data))) {
	    	if (context->last_reason == NULL)
			context->last_reason = talloc_strdup(context, "Setup failure");
		context->last_result = TORTURE_ERROR;
		success = false;
	} else if (test->dangerous && 
	    !torture_setting_bool(context, "dangerous", false)) {
	    context->last_result = TORTURE_SKIP;
	    context->last_reason = talloc_asprintf(context, 
	    	"disabled %s - enable dangerous tests to use", test->name);
	    success = true;
	} else {
	    success = test->run(context, tcase, test);

	    if (!success && context->last_result == TORTURE_OK) {
		    if (context->last_reason == NULL)
			    context->last_reason = talloc_strdup(context, "Unknown error/failure");
		    context->last_result = TORTURE_ERROR;
	    }
	}

	if (!already_setup && tcase->teardown && !tcase->teardown(context, tcase->data)) {
    		if (context->last_reason == NULL)
		    context->last_reason = talloc_strdup(context, "Setup failure");
	    	context->last_result = TORTURE_ERROR;
		success = false;
	}

	torture_ui_test_result(context, context->last_result, 
			       context->last_reason);
	
	talloc_free(context->last_reason);

	if (tcase == NULL || strcmp(test->name, tcase->name) != 0) { 
		talloc_free(context->active_testname);
		context->active_testname = old_testname;
	}
	context->active_test = NULL;
	context->active_tcase = NULL;

	return success;
}

bool torture_run_tcase(struct torture_context *context, 
		       struct torture_tcase *tcase)
{
	bool ret = true;
	char *old_testname;
	struct torture_test *test;

	context->active_tcase = tcase;
	if (context->results->ui_ops->tcase_start) 
		context->results->ui_ops->tcase_start(context, tcase);

	if (tcase->fixture_persistent && tcase->setup 
		&& !tcase->setup(context, &tcase->data)) {
		/* FIXME: Use torture ui ops for reporting this error */
		fprintf(stderr, "Setup failed: ");
		if (context->last_reason != NULL)
			fprintf(stderr, "%s", context->last_reason);
		fprintf(stderr, "\n");
		ret = false;
		goto done;
	}

	old_testname = context->active_testname;
	context->active_testname = talloc_asprintf(context, "%s-%s", 
						   old_testname, tcase->name);
	for (test = tcase->tests; test; test = test->next) {
		ret &= internal_torture_run_test(context, tcase, test, 
				tcase->fixture_persistent);
	}
	talloc_free(context->active_testname);
	context->active_testname = old_testname;

	if (tcase->fixture_persistent && tcase->teardown &&
		!tcase->teardown(context, tcase->data))
		ret = false;

done:
	context->active_tcase = NULL;

	if (context->results->ui_ops->tcase_finish)
		context->results->ui_ops->tcase_finish(context, tcase);

	return ret;
}

bool torture_run_test(struct torture_context *context, 
					  struct torture_tcase *tcase,
					  struct torture_test *test)
{
	return internal_torture_run_test(context, tcase, test, false);
}

int torture_setting_int(struct torture_context *test, const char *name, 
							int default_value)
{
	return lp_parm_int(test->lp_ctx, NULL, "torture", name, default_value);
}

double torture_setting_double(struct torture_context *test, const char *name, 
							double default_value)
{
	return lp_parm_double(test->lp_ctx, NULL, "torture", name, default_value);
}

bool torture_setting_bool(struct torture_context *test, const char *name, 
							bool default_value)
{
	return lp_parm_bool(test->lp_ctx, NULL, "torture", name, default_value);
}

const char *torture_setting_string(struct torture_context *test, 
				   const char *name, 
				   const char *default_value)
{
	const char *ret;

	SMB_ASSERT(test != NULL);
	SMB_ASSERT(test->lp_ctx != NULL);
	
	ret = lp_parm_string(test->lp_ctx, NULL, "torture", name);

	if (ret == NULL)
		return default_value;

	return ret;
}

static bool wrap_test_with_simple_tcase_const (
		struct torture_context *torture_ctx,
		struct torture_tcase *tcase,
		struct torture_test *test)
{
	bool (*fn) (struct torture_context *, const void *tcase_data);

	fn = test->fn;

	return fn(torture_ctx, test->data);
}

struct torture_tcase *torture_suite_add_simple_tcase_const(
		struct torture_suite *suite, const char *name,
		bool (*run) (struct torture_context *test, const void *),
		const void *data)
{
	struct torture_tcase *tcase;
	struct torture_test *test;

	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_test_with_simple_tcase_const;
	test->fn = run;
	test->data = data;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return tcase;
}

static bool wrap_simple_test(struct torture_context *torture_ctx,
			     struct torture_tcase *tcase,
			     struct torture_test *test)
{
	bool (*fn) (struct torture_context *);

	fn = test->fn;

	return fn(torture_ctx);
}

struct torture_tcase *torture_suite_add_simple_test(
					struct torture_suite *suite, 
					const char *name,
					bool (*run) (struct torture_context *test))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_simple_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return tcase;
}

/**
 * Add a child testsuite to a testsuite.
 */
bool torture_suite_add_suite(struct torture_suite *suite, 
			     struct torture_suite *child)
{
	if (child == NULL)
		return false;

	DLIST_ADD_END(suite->children, child, struct torture_suite *);

	/* FIXME: Check for duplicates and return false if the 
	 * added suite already exists as a child */

	return true;
}

/**
 * Find the child testsuite with the specified name.
 */
struct torture_suite *torture_find_suite(struct torture_suite *parent, 
					 const char *name)
{
	struct torture_suite *child;

	for (child = parent->children; child; child = child->next) 
		if (!strcmp(child->name, name))
			return child;

	return NULL;
}

static bool wrap_test_with_simple_test_const(struct torture_context *torture_ctx,
				       struct torture_tcase *tcase,
				       struct torture_test *test)
{
	bool (*fn) (struct torture_context *, const void *tcase_data);

	fn = test->fn;

	return fn(torture_ctx, tcase->data);
}

struct torture_test *torture_tcase_add_simple_test_const(
		struct torture_tcase *tcase,
		const char *name,
		bool (*run) (struct torture_context *test,
			const void *tcase_data))
{
	struct torture_test *test;

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_test_with_simple_test_const;
	test->fn = run;
	test->data = NULL;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}

static bool wrap_test_with_simple_test(struct torture_context *torture_ctx,
				       struct torture_tcase *tcase,
				       struct torture_test *test)
{
	bool (*fn) (struct torture_context *, void *tcase_data);

	fn = test->fn;

	return fn(torture_ctx, tcase->data);
}

struct torture_test *torture_tcase_add_simple_test(struct torture_tcase *tcase,
		const char *name,
		bool (*run) (struct torture_context *test, void *tcase_data))
{
	struct torture_test *test;

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_test_with_simple_test;
	test->fn = run;
	test->data = NULL;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}
