/* tinytest.c -- Copyright 2009-2012 Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef TINYTEST_LOCAL
#include "tinytest_local.h"
#endif

#ifdef WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#if (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060 && \
    __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1070)
/* Workaround for a stupid bug in OSX 10.6 */
#define FORK_BREAKS_GCOV
#include <vproc.h>
#endif
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#include "tinytest.h"
#include "tinytest_macros.h"

#define LONGEST_TEST_NAME 16384

static int in_tinytest_main = 0; /**< true if we're in tinytest_main().*/
static int n_ok = 0; /**< Number of tests that have passed */
static int n_bad = 0; /**< Number of tests that have failed. */
static int n_skipped = 0; /**< Number of tests that have been skipped. */

static int opt_forked = 0; /**< True iff we're called from inside a win32 fork*/
static int opt_nofork = 0; /**< Suppress calls to fork() for debugging. */
static int opt_verbosity = 1; /**< -==quiet,0==terse,1==normal,2==verbose */
const char *verbosity_flag = "";

enum outcome { SKIP=2, OK=1, FAIL=0 };
static enum outcome cur_test_outcome = 0;
const char *cur_test_prefix = NULL; /**< prefix of the current test group */
/** Name of the current test, if we haven't logged is yet. Used for --quiet */
const char *cur_test_name = NULL;

#ifdef WIN32
/* Copy of argv[0] for win32. */
static char commandname[MAX_PATH+1];
#endif

static void usage(struct testgroup_t *groups, int list_groups)
  __attribute__((noreturn));

static enum outcome
_testcase_run_bare(const struct testcase_t *testcase)
{
	void *env = NULL;
	int outcome;
	if (testcase->setup) {
		env = testcase->setup->setup_fn(testcase);
		if (!env)
			return FAIL;
		else if (env == (void*)TT_SKIP)
			return SKIP;
	}

	cur_test_outcome = OK;
	testcase->fn(env);
	outcome = cur_test_outcome;

	if (testcase->setup) {
		if (testcase->setup->cleanup_fn(testcase, env) == 0)
			outcome = FAIL;
	}

	return outcome;
}

#define MAGIC_EXITCODE 42

static enum outcome
_testcase_run_forked(const struct testgroup_t *group,
		     const struct testcase_t *testcase)
{
#ifdef WIN32
	/* Fork? On Win32?  How primitive!  We'll do what the smart kids do:
	   we'll invoke our own exe (whose name we recall from the command
	   line) with a command line that tells it to run just the test we
	   want, and this time without forking.

	   (No, threads aren't an option.  The whole point of forking is to
	   share no state between tests.)
	 */
	int ok;
	char buffer[LONGEST_TEST_NAME+256];
	STARTUPINFOA si;
	PROCESS_INFORMATION info;
	DWORD exitcode;

	if (!in_tinytest_main) {
		printf("\nERROR.  On Windows, _testcase_run_forked must be"
		       " called from within tinytest_main.\n");
		abort();
	}
	if (opt_verbosity>0)
		printf("[forking] ");

	snprintf(buffer, sizeof(buffer), "%s --RUNNING-FORKED %s %s%s",
		 commandname, verbosity_flag, group->prefix, testcase->name);

	memset(&si, 0, sizeof(si));
	memset(&info, 0, sizeof(info));
	si.cb = sizeof(si);

	ok = CreateProcessA(commandname, buffer, NULL, NULL, 0,
			   0, NULL, NULL, &si, &info);
	if (!ok) {
		printf("CreateProcess failed!\n");
		return 0;
	}
	WaitForSingleObject(info.hProcess, INFINITE);
	GetExitCodeProcess(info.hProcess, &exitcode);
	CloseHandle(info.hProcess);
	CloseHandle(info.hThread);
	if (exitcode == 0)
		return OK;
	else if (exitcode == MAGIC_EXITCODE)
		return SKIP;
	else
		return FAIL;
#else
	int outcome_pipe[2];
	pid_t pid;
	(void)group;

	if (pipe(outcome_pipe))
		perror("opening pipe");

	if (opt_verbosity>0)
		printf("[forking] ");
	pid = fork();
#ifdef FORK_BREAKS_GCOV
	vproc_transaction_begin(0);
#endif
	if (!pid) {
		/* child. */
		int test_r, write_r;
		char b[1];
		close(outcome_pipe[0]);
		test_r = _testcase_run_bare(testcase);
		assert(0<=(int)test_r && (int)test_r<=2);
		b[0] = "NYS"[test_r];
		write_r = (int)write(outcome_pipe[1], b, 1);
		if (write_r != 1) {
			perror("write outcome to pipe");
			exit(1);
		}
		exit(0);
		return FAIL; /* unreachable */
	} else {
		/* parent */
		int status, r;
		char b[1];
		/* Close this now, so that if the other side closes it,
		 * our read fails. */
		close(outcome_pipe[1]);
		r = (int)read(outcome_pipe[0], b, 1);
		if (r == 0) {
			printf("[Lost connection!] ");
			return 0;
		} else if (r != 1) {
			perror("read outcome from pipe");
		}
		waitpid(pid, &status, 0);
		close(outcome_pipe[0]);
		return b[0]=='Y' ? OK : (b[0]=='S' ? SKIP : FAIL);
	}
#endif
}

int
testcase_run_one(const struct testgroup_t *group,
		 const struct testcase_t *testcase)
{
	enum outcome outcome;

	if (testcase->flags & TT_SKIP) {
		if (opt_verbosity>0)
			printf("%s%s: SKIPPED\n",
			    group->prefix, testcase->name);
		++n_skipped;
		return SKIP;
	}

	if (opt_verbosity>0 && !opt_forked) {
		printf("%s%s: ", group->prefix, testcase->name);
	} else {
		if (opt_verbosity==0) printf(".");
		cur_test_prefix = group->prefix;
		cur_test_name = testcase->name;
	}

	if ((testcase->flags & TT_FORK) && !(opt_forked||opt_nofork)) {
		outcome = _testcase_run_forked(group, testcase);
	} else {
		outcome = _testcase_run_bare(testcase);
	}

	if (outcome == OK) {
		++n_ok;
		if (opt_verbosity>0 && !opt_forked)
			puts(opt_verbosity==1?"OK":"");
	} else if (outcome == SKIP) {
		++n_skipped;
		if (opt_verbosity>0 && !opt_forked)
			puts("SKIPPED");
	} else {
		++n_bad;
		if (!opt_forked)
			printf("\n  [%s FAILED]\n", testcase->name);
	}

	if (opt_forked) {
		exit(outcome==OK ? 0 : (outcome==SKIP?MAGIC_EXITCODE : 1));
		return 1; /* unreachable */
	} else {
		return (int)outcome;
	}
}

int
_tinytest_set_flag(struct testgroup_t *groups, const char *arg, unsigned long flag)
{
	int i, j;
	size_t length = LONGEST_TEST_NAME;
	char fullname[LONGEST_TEST_NAME];
	int found=0;
	if (strstr(arg, ".."))
		length = strstr(arg,"..")-arg;
	for (i=0; groups[i].prefix; ++i) {
		for (j=0; groups[i].cases[j].name; ++j) {
			snprintf(fullname, sizeof(fullname), "%s%s",
				 groups[i].prefix, groups[i].cases[j].name);
			if (!flag) /* Hack! */
				printf("    %s\n", fullname);
			if (!strncmp(fullname, arg, length)) {
				groups[i].cases[j].flags |= flag;
				++found;
			}
		}
	}
	return found;
}

static void
usage(struct testgroup_t *groups, int list_groups)
{
	puts("Options are: [--verbose|--quiet|--terse] [--no-fork]");
	puts("  Specify tests by name, or using a prefix ending with '..'");
	puts("  To skip a test, list give its name prefixed with a colon.");
	puts("  Use --list-tests for a list of tests.");
	if (list_groups) {
		puts("Known tests are:");
		_tinytest_set_flag(groups, "..", 0);
	}
	exit(0);
}

int
tinytest_main(int c, const char **v, struct testgroup_t *groups)
{
	int i, j, n=0;

#ifdef WIN32
	const char *sp = strrchr(v[0], '.');
	const char *extension = "";
	if (!sp || stricmp(sp, ".exe"))
		extension = ".exe"; /* Add an exe so CreateProcess will work */
	snprintf(commandname, sizeof(commandname), "%s%s", v[0], extension);
	commandname[MAX_PATH]='\0';
#endif
	for (i=1; i<c; ++i) {
		if (v[i][0] == '-') {
			if (!strcmp(v[i], "--RUNNING-FORKED")) {
				opt_forked = 1;
			} else if (!strcmp(v[i], "--no-fork")) {
				opt_nofork = 1;
			} else if (!strcmp(v[i], "--quiet")) {
				opt_verbosity = -1;
				verbosity_flag = "--quiet";
			} else if (!strcmp(v[i], "--verbose")) {
				opt_verbosity = 2;
				verbosity_flag = "--verbose";
			} else if (!strcmp(v[i], "--terse")) {
				opt_verbosity = 0;
				verbosity_flag = "--terse";
			} else if (!strcmp(v[i], "--help")) {
				usage(groups, 0);
			} else if (!strcmp(v[i], "--list-tests")) {
				usage(groups, 1);
			} else {
				printf("Unknown option %s.  Try --help\n",v[i]);
				return -1;
			}
		} else {
			const char *test = v[i];
			int flag = _TT_ENABLED;
			if (test[0] == ':') {
				++test;
				flag = TT_SKIP;
			} else {
				++n;
			}
			if (!_tinytest_set_flag(groups, test, flag)) {
				printf("No such test as %s!\n", v[i]);
				return -1;
			}
		}
	}
	if (!n)
		_tinytest_set_flag(groups, "..", _TT_ENABLED);

	setvbuf(stdout, NULL, _IONBF, 0);

	++in_tinytest_main;
	for (i=0; groups[i].prefix; ++i)
		for (j=0; groups[i].cases[j].name; ++j)
			if (groups[i].cases[j].flags & _TT_ENABLED)
				testcase_run_one(&groups[i],
						 &groups[i].cases[j]);

	--in_tinytest_main;

	if (opt_verbosity==0)
		puts("");

	if (n_bad)
		printf("%d/%d TESTS FAILED. (%d skipped)\n", n_bad,
		       n_bad+n_ok,n_skipped);
	else if (opt_verbosity >= 1)
		printf("%d tests ok.  (%d skipped)\n", n_ok, n_skipped);

	return (n_bad == 0) ? 0 : 1;
}

int
_tinytest_get_verbosity(void)
{
	return opt_verbosity;
}

void
_tinytest_set_test_failed(void)
{
	if (opt_verbosity <= 0 && cur_test_name) {
		if (opt_verbosity==0) puts("");
		printf("%s%s: ", cur_test_prefix, cur_test_name);
		cur_test_name = NULL;
	}
	cur_test_outcome = 0;
}

void
_tinytest_set_test_skipped(void)
{
	if (cur_test_outcome==OK)
		cur_test_outcome = SKIP;
}

