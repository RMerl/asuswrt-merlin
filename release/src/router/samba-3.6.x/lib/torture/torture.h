/* 
   Unix SMB/CIFS implementation.
   SMB torture UI functions

   Copyright (C) Jelmer Vernooij 2006
   
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

#ifndef __TORTURE_UI_H__
#define __TORTURE_UI_H__

struct torture_test;
struct torture_context;
struct torture_suite;
struct torture_tcase;
struct torture_results;

enum torture_result { 
	TORTURE_OK=0, 
	TORTURE_FAIL=1,
	TORTURE_ERROR=2,
	TORTURE_SKIP=3
};

enum torture_progress_whence {
	TORTURE_PROGRESS_SET,
	TORTURE_PROGRESS_CUR,
	TORTURE_PROGRESS_POP,
	TORTURE_PROGRESS_PUSH,
};

/* 
 * These callbacks should be implemented by any backend that wishes 
 * to listen to reports from the torture tests.
 */
struct torture_ui_ops
{
	void (*init) (struct torture_results *);
	void (*comment) (struct torture_context *, const char *);
	void (*warning) (struct torture_context *, const char *);
	void (*suite_start) (struct torture_context *, struct torture_suite *);
	void (*suite_finish) (struct torture_context *, struct torture_suite *);
	void (*tcase_start) (struct torture_context *, struct torture_tcase *); 
	void (*tcase_finish) (struct torture_context *, struct torture_tcase *);
	void (*test_start) (struct torture_context *, 
						struct torture_tcase *,
						struct torture_test *);
	void (*test_result) (struct torture_context *, 
						 enum torture_result, const char *reason);
	void (*progress) (struct torture_context *, int offset, enum torture_progress_whence whence);
	void (*report_time) (struct torture_context *);
};

void torture_ui_test_start(struct torture_context *context,
							   struct torture_tcase *tcase,
							   struct torture_test *test);

void torture_ui_test_result(struct torture_context *context,
								enum torture_result result,
								const char *comment);

void torture_ui_report_time(struct torture_context *context);

/*
 * Holds information about a specific run of the testsuite. 
 * The data in this structure should be considered private to 
 * the torture tests and should only be used directly by the torture 
 * code and the ui backends.
 *
 * Torture tests should instead call the torture_*() macros and functions 
 * specified below.
 */

struct torture_context
{
	struct torture_results *results;

	struct torture_test *active_test;
	struct torture_tcase *active_tcase;

	enum torture_result last_result;
	char *last_reason;

	/** Directory used for temporary test data */
	const char *outputdir;

	/** Event context */
	struct tevent_context *ev;

	/** Loadparm context (will go away in favor of torture_setting_ at some point) */
	struct loadparm_context *lp_ctx;
};

struct torture_results
{
	const struct torture_ui_ops *ui_ops;
	void *ui_data;

	/** Whether tests should avoid writing output to stdout */
	bool quiet;

	bool returncode;
};

/* 
 * Describes a particular torture test
 */
struct torture_test {
	/** Short unique name for the test. */
	const char *name;

	/** Long description for the test. */
	const char *description;

	/** Whether this is a dangerous test 
	 * (can corrupt the remote servers data or bring it down). */
	bool dangerous;

	/** Function to call to run this test */
	bool (*run) (struct torture_context *torture_ctx, 
				 struct torture_tcase *tcase,
				 struct torture_test *test);

	struct torture_test *prev, *next;

	/** Pointer to the actual test function. This is run by the 
	  * run() function above. */
	void *fn;

	/** Use data for this test */
	const void *data;
};

/* 
 * Describes a particular test case.
 */
struct torture_tcase {
    const char *name;
	const char *description;
	bool (*setup) (struct torture_context *tcase, void **data);
	bool (*teardown) (struct torture_context *tcase, void *data); 
	bool fixture_persistent;
	void *data;
	struct torture_test *tests;
	struct torture_tcase *prev, *next;
};

struct torture_suite
{
	const char *name;
	const char *description;
	struct torture_tcase *testcases;
	struct torture_suite *children;

	/* Pointers to siblings of this torture suite */
	struct torture_suite *prev, *next;
};

/** Create a new torture suite */
struct torture_suite *torture_suite_create(TALLOC_CTX *mem_ctx,
		const char *name);

/** Change the setup and teardown functions for a testcase */
void torture_tcase_set_fixture(struct torture_tcase *tcase,
		bool (*setup) (struct torture_context *, void **),
		bool (*teardown) (struct torture_context *, void *));

/* Add another test to run for a particular testcase */
struct torture_test *torture_tcase_add_test_const(struct torture_tcase *tcase,
		const char *name,
		bool (*run) (struct torture_context *test,
			const void *tcase_data, const void *test_data),
		const void *test_data);

/* Add a testcase to a testsuite */
struct torture_tcase *torture_suite_add_tcase(struct torture_suite *suite,
							 const char *name);

/* Convenience wrapper that adds a testcase against only one
 * test will be run */
struct torture_tcase *torture_suite_add_simple_tcase_const(
		struct torture_suite *suite,
		const char *name,
		bool (*run) (struct torture_context *test,
			const void *test_data),
		const void *data);

/* Convenience function that adds a test which only
 * gets the test case data */
struct torture_test *torture_tcase_add_simple_test_const(
		struct torture_tcase *tcase,
		const char *name,
		bool (*run) (struct torture_context *test,
			const void *tcase_data));

/* Convenience wrapper that adds a test that doesn't need any
 * testcase data */
struct torture_tcase *torture_suite_add_simple_test(
		struct torture_suite *suite,
		const char *name,
		bool (*run) (struct torture_context *test));

/* Add a child testsuite to an existing testsuite */
bool torture_suite_add_suite(struct torture_suite *suite,
		struct torture_suite *child);

/* Run the specified testsuite recursively */
bool torture_run_suite(struct torture_context *context,
					   struct torture_suite *suite);

/* Run the specified testsuite recursively, but only the specified 
 * tests */
bool torture_run_suite_restricted(struct torture_context *context, 
		       struct torture_suite *suite, const char **restricted);

/* Run the specified testcase */
bool torture_run_tcase(struct torture_context *context,
					   struct torture_tcase *tcase);

bool torture_run_tcase_restricted(struct torture_context *context, 
		       struct torture_tcase *tcase, const char **restricted);

/* Run the specified test */
bool torture_run_test(struct torture_context *context,
					  struct torture_tcase *tcase,
					  struct torture_test *test);

bool torture_run_test_restricted(struct torture_context *context,
					  struct torture_tcase *tcase,
					  struct torture_test *test,
					  const char **restricted);

void torture_comment(struct torture_context *test, const char *comment, ...) PRINTF_ATTRIBUTE(2,3);
void torture_warning(struct torture_context *test, const char *comment, ...) PRINTF_ATTRIBUTE(2,3);
void torture_result(struct torture_context *test,
			enum torture_result, const char *reason, ...) PRINTF_ATTRIBUTE(3,4);

#define torture_assert(torture_ctx,expr,cmt) \
	if (!(expr)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": Expression `%s' failed: %s", __STRING(expr), cmt); \
		return false; \
	}

#define torture_assert_goto(torture_ctx,expr,ret,label,cmt) \
	if (!(expr)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": Expression `%s' failed: %s", __STRING(expr), cmt); \
		ret = false; \
		goto label; \
	}

#define torture_assert_werr_equal(torture_ctx, got, expected, cmt) \
	do { WERROR __got = got, __expected = expected; \
	if (!W_ERROR_EQUAL(__got, __expected)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %s, expected %s: %s", win_errstr(__got), win_errstr(__expected), cmt); \
		return false; \
	} \
	} while (0)

#define torture_assert_ntstatus_equal(torture_ctx,got,expected,cmt) \
	do { NTSTATUS __got = got, __expected = expected; \
	if (!NT_STATUS_EQUAL(__got, __expected)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %s, expected %s: %s", nt_errstr(__got), nt_errstr(__expected), cmt); \
		return false; \
	}\
	} while(0)

#define torture_assert_ntstatus_equal_goto(torture_ctx,got,expected,ret,label,cmt) \
	do { NTSTATUS __got = got, __expected = expected; \
	if (!NT_STATUS_EQUAL(__got, __expected)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %s, expected %s: %s", nt_errstr(__got), nt_errstr(__expected), cmt); \
		ret = false; \
		goto label; \
	}\
	} while(0)

#define torture_assert_ndr_err_equal(torture_ctx,got,expected,cmt) \
	do { enum ndr_err_code __got = got, __expected = expected; \
	if (__got != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %d, expected %d (%s): %s", __got, __expected, __STRING(expected), cmt); \
		return false; \
	}\
	} while(0)

#define torture_assert_casestr_equal(torture_ctx,got,expected,cmt) \
	do { const char *__got = (got), *__expected = (expected); \
	if (!strequal(__got, __expected)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %s, expected %s: %s", __got, __expected, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_str_equal(torture_ctx,got,expected,cmt)\
	do { const char *__got = (got), *__expected = (expected); \
	if (strcmp_safe(__got, __expected) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
					   __location__": "#got" was %s, expected %s: %s", \
					   __got, __expected, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_strn_equal(torture_ctx,got,expected,len,cmt)\
	do { const char *__got = (got), *__expected = (expected); \
	if (strncmp(__got, __expected, len) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
					   __location__": "#got" %s of len %d did not match "#expected" %s: %s", \
					   __got, (int)len, __expected, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_str_equal_goto(torture_ctx,got,expected,ret,label,cmt)\
	do { const char *__got = (got), *__expected = (expected); \
	if (strcmp_safe(__got, __expected) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
					   __location__": "#got" was %s, expected %s: %s", \
					   __got, __expected, cmt); \
		ret = false; \
		goto label; \
	} \
	} while(0)

#define torture_assert_mem_equal(torture_ctx,got,expected,len,cmt)\
	do { const void *__got = (got), *__expected = (expected); \
	if (memcmp(__got, __expected, len) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			       __location__": "#got" of len %d did not match "#expected": %s", (int)len, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_data_blob_equal(torture_ctx,got,expected,cmt)\
	do { const DATA_BLOB __got = (got), __expected = (expected); \
	if (__got.length != __expected.length) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			       __location__": "#got".len %d did not match "#expected" len %d: %s", \
			       (int)__got.length, (int)__expected.length, cmt); \
		return false; \
	} \
	if (memcmp(__got.data, __expected.data, __got.length) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			       __location__": "#got" of len %d did not match "#expected": %s", (int)__got.length, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_file_contains_text(torture_ctx,filename,expected,cmt)\
	do { \
	char *__got; \
	const char *__expected = (expected); \
	size_t __size; \
	__got = file_load(filename, &__size, 0, torture_ctx); \
	if (__got == NULL) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			       __location__": unable to open %s: %s\n", \
			       filename, cmt); \
		return false; \
	} \
	\
	if (strcmp_safe(__got, __expected) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": %s contained:\n%sExpected: %s%s\n", \
			filename, __got, __expected, cmt); \
		talloc_free(__got); \
		return false; \
	} \
	talloc_free(__got); \
	} while(0)

#define torture_assert_file_contains(torture_ctx,filename,expected,cmt)\
	do { const char *__got, *__expected = (expected); \
	size_t __size; \
	__got = file_load(filename, *size, 0, torture_ctx); \
	if (strcmp_safe(__got, __expected) != 0) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
					   __location__": %s contained:\n%sExpected: %s%s\n", \
					   __got, __expected, cmt); \
		talloc_free(__got); \
		return false; \
	} \
	talloc_free(__got); \
	} while(0)

#define torture_assert_int_equal(torture_ctx,got,expected,cmt)\
	do { int __got = (got), __expected = (expected); \
	if (__got != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" was %d (0x%X), expected %d (0x%X): %s", \
			__got, __got, __expected, __expected, cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_int_equal_goto(torture_ctx,got,expected,ret,label,cmt)\
	do { int __got = (got), __expected = (expected); \
	if (__got != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" was %d (0x%X), expected %d (0x%X): %s", \
			__got, __got, __expected, __expected, cmt); \
		ret = false; \
		goto label; \
	} \
	} while(0)

#define torture_assert_u64_equal(torture_ctx,got,expected,cmt)\
	do { uint64_t __got = (got), __expected = (expected); \
	if (__got != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" was %llu (0x%llX), expected %llu (0x%llX): %s", \
			(unsigned long long)__got, (unsigned long long)__got, \
			(unsigned long long)__expected, (unsigned long long)__expected, \
			cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_u64_equal_goto(torture_ctx,got,expected,ret,label,cmt)\
	do { uint64_t __got = (got), __expected = (expected); \
	if (__got != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": "#got" was %llu (0x%llX), expected %llu (0x%llX): %s", \
			(unsigned long long)__got, (unsigned long long)__got, \
			(unsigned long long)__expected, (unsigned long long)__expected, \
			cmt); \
		ret = false; \
		goto label; \
	} \
	} while(0)

#define torture_assert_errno_equal(torture_ctx,expected,cmt)\
	do { int __expected = (expected); \
	if (errno != __expected) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			__location__": errno was %d (%s), expected %d: %s: %s", \
					   errno, strerror(errno), __expected, \
					   strerror(__expected), cmt); \
		return false; \
	} \
	} while(0)

#define torture_assert_nttime_equal(torture_ctx,got,expected,cmt) \
	do { NTTIME __got = got, __expected = expected; \
	if (!nt_time_equal(&__got, &__expected)) { \
		torture_result(torture_ctx, TORTURE_FAIL, __location__": "#got" was %s, expected %s: %s", nt_time_string(tctx, __got), nt_time_string(tctx, __expected), cmt); \
		return false; \
	}\
	} while(0)

#define torture_skip(torture_ctx,cmt) do {\
		torture_result(torture_ctx, TORTURE_SKIP, __location__": %s", cmt);\
		return true; \
	} while(0)
#define torture_skip_goto(torture_ctx,label,cmt) do {\
		torture_result(torture_ctx, TORTURE_SKIP, __location__": %s", cmt);\
		goto label; \
	} while(0)
#define torture_fail(torture_ctx,cmt) do {\
		torture_result(torture_ctx, TORTURE_FAIL, __location__": %s", cmt);\
		return false; \
	} while (0)
#define torture_fail_goto(torture_ctx,label,cmt) do {\
		torture_result(torture_ctx, TORTURE_FAIL, __location__": %s", cmt);\
		goto label; \
	} while (0)

#define torture_out stderr

/* Convenience macros */
#define torture_assert_ntstatus_ok(torture_ctx,expr,cmt) \
		torture_assert_ntstatus_equal(torture_ctx,expr,NT_STATUS_OK,cmt)

#define torture_assert_ntstatus_ok_goto(torture_ctx,expr,ret,label,cmt) \
		torture_assert_ntstatus_equal_goto(torture_ctx,expr,NT_STATUS_OK,ret,label,cmt)

#define torture_assert_werr_ok(torture_ctx,expr,cmt) \
		torture_assert_werr_equal(torture_ctx,expr,WERR_OK,cmt)

#define torture_assert_ndr_success(torture_ctx,expr,cmt) \
		torture_assert_ndr_err_equal(torture_ctx,expr,NDR_ERR_SUCCESS,cmt)

/* Getting settings */
const char *torture_setting_string(struct torture_context *test, \
								   const char *name, 
								   const char *default_value);

int torture_setting_int(struct torture_context *test, 
						const char *name, 
						int default_value);

double torture_setting_double(struct torture_context *test, 
						const char *name, 
						double default_value);

bool torture_setting_bool(struct torture_context *test, 
						  const char *name, 
						  bool default_value);

struct torture_suite *torture_find_suite(struct torture_suite *parent, 
										 const char *name);

unsigned long torture_setting_ulong(struct torture_context *test,
				    const char *name,
				    unsigned long default_value);

NTSTATUS torture_temp_dir(struct torture_context *tctx, 
				   const char *prefix, 
				   char **tempdir);
NTSTATUS torture_deltree_outputdir(struct torture_context *tctx);

struct torture_test *torture_tcase_add_simple_test(struct torture_tcase *tcase,
		const char *name,
		bool (*run) (struct torture_context *test, void *tcase_data));


bool torture_suite_init_tcase(struct torture_suite *suite, 
			      struct torture_tcase *tcase, 
			      const char *name);
int torture_suite_children_count(const struct torture_suite *suite);

struct torture_context *torture_context_init(struct tevent_context *event_ctx, struct torture_results *results);

struct torture_results *torture_results_init(TALLOC_CTX *mem_ctx, const struct torture_ui_ops *ui_ops);

struct torture_context *torture_context_child(struct torture_context *tctx);

extern const struct torture_ui_ops torture_subunit_ui_ops;

#endif /* __TORTURE_UI_H__ */
