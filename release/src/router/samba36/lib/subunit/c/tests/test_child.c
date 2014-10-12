/**
 *
 *  subunit C bindings.
 *  Copyright (C) 2006  Robert Collins <robertc@robertcollins.net>
 *
 *  Licensed under either the Apache License, Version 2.0 or the BSD 3-clause
 *  license at the users choice. A copy of both licenses are available in the
 *  project source as Apache-2.0 and BSD. You may not use this file except in
 *  compliance with one of these two licences.
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under these licenses is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the license you chose for the specific language governing permissions
 *  and limitations under that license.
 **/

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <check.h>

#include "subunit/child.h"

/**
 * Helper function to capture stdout, run some call, and check what
 * was written.
 * @expected the expected stdout content
 * @function the function to call.
 **/
static void
test_stdout_function(char const * expected,
                     void (*function)(void))
{
    /* test that the start function emits a correct test: line. */
    int bytecount;
    int old_stdout;
    int new_stdout[2];
    char buffer[100];
    /* we need a socketpair to capture stdout in */
    fail_if(pipe(new_stdout), "Failed to create a socketpair.");
    /* backup stdout so we can replace it */
    old_stdout = dup(1);
    if (old_stdout == -1) {
      close(new_stdout[0]);
      close(new_stdout[1]);
      fail("Failed to backup stdout before replacing.");
    }
    /* redirect stdout so we can analyse it */
    if (dup2(new_stdout[1], 1) != 1) {
      close(old_stdout);
      close(new_stdout[0]);
      close(new_stdout[1]);
      fail("Failed to redirect stdout");
    }
    /* yes this can block. Its a test case with < 100 bytes of output.
     * DEAL.
     */
    function();
    /* restore stdout now */
    if (dup2(old_stdout, 1) != 1) {
      close(old_stdout);
      close(new_stdout[0]);
      close(new_stdout[1]);
      fail("Failed to restore stdout");
    }
    /* and we dont need the write side any more */
    if (close(new_stdout[1])) {
      close(new_stdout[0]);
      fail("Failed to close write side of socketpair.");
    }
    /* get the output */
    bytecount = read(new_stdout[0], buffer, 100);
    if (0 > bytecount) {
      close(new_stdout[0]);
      fail("Failed to read captured output.");
    }
    buffer[bytecount]='\0';
    /* and we dont need the read side any more */
    fail_if(close(new_stdout[0]), "Failed to close write side of socketpair.");
    /* compare with expected outcome */
    fail_if(strcmp(expected, buffer), "Did not get expected output [%s], got [%s]", expected, buffer);
}


static void
call_test_start(void)
{
    subunit_test_start("test case");
}


START_TEST (test_start)
{
    test_stdout_function("test: test case\n", call_test_start);
}
END_TEST


static void
call_test_pass(void)
{
    subunit_test_pass("test case");
}


START_TEST (test_pass)
{
    test_stdout_function("success: test case\n", call_test_pass);
}
END_TEST


static void
call_test_fail(void)
{
    subunit_test_fail("test case", "Multiple lines\n of error\n");
}


START_TEST (test_fail)
{
    test_stdout_function("failure: test case [\n"
                         "Multiple lines\n"
        		 " of error\n"
			 "]\n",
			 call_test_fail);
}
END_TEST


static void
call_test_error(void)
{
    subunit_test_error("test case", "Multiple lines\n of output\n");
}


START_TEST (test_error)
{
    test_stdout_function("error: test case [\n"
                         "Multiple lines\n"
        		 " of output\n"
			 "]\n",
			 call_test_error);
}
END_TEST


static void
call_test_skip(void)
{
    subunit_test_skip("test case", "Multiple lines\n of output\n");
}


START_TEST (test_skip)
{
    test_stdout_function("skip: test case [\n"
                         "Multiple lines\n"
        		 " of output\n"
			 "]\n",
			 call_test_skip);
}
END_TEST


static void
call_test_progress_pop(void)
{
	subunit_progress(SUBUNIT_PROGRESS_POP, 0);
}

static void
call_test_progress_set(void)
{
	subunit_progress(SUBUNIT_PROGRESS_SET, 5);
}

static void
call_test_progress_push(void)
{
	subunit_progress(SUBUNIT_PROGRESS_PUSH, 0);
}

static void
call_test_progress_cur(void)
{
	subunit_progress(SUBUNIT_PROGRESS_CUR, -6);
}

START_TEST (test_progress)
{
	test_stdout_function("progress: pop\n",
			 call_test_progress_pop);
	test_stdout_function("progress: push\n",
			 call_test_progress_push);
	test_stdout_function("progress: 5\n",
			 call_test_progress_set);
	test_stdout_function("progress: -6\n",
			 call_test_progress_cur);
}
END_TEST

static Suite *
child_suite(void)
{
    Suite *s = suite_create("subunit_child");
    TCase *tc_core = tcase_create("Core");
    suite_add_tcase (s, tc_core);
    tcase_add_test (tc_core, test_start);
    tcase_add_test (tc_core, test_pass);
    tcase_add_test (tc_core, test_fail);
    tcase_add_test (tc_core, test_error);
    tcase_add_test (tc_core, test_skip);
    tcase_add_test (tc_core, test_progress);
    return s;
}


int
main(void)
{
  int nf;
  Suite *s = child_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
