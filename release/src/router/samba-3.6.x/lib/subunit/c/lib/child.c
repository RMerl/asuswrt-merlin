/**
 *
 *  subunit C child-side bindings: report on tests being run.
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

#include <stdio.h>
#include <string.h>
#include "subunit/child.h"

/* Write details about a test event. It is the callers responsibility to ensure
 * that details are only provided for events the protocol expects details on.
 * @event: The event - e.g. 'skip'
 * @name: The test name/id.
 * @details: The details of the event, may be NULL if no details are present.
 */
static void
subunit_send_event(char const * const event, char const * const name,
		   char const * const details)
{
  if (NULL == details) {
    fprintf(stdout, "%s: %s\n", event, name);
  } else {
    fprintf(stdout, "%s: %s [\n", event, name);
    fprintf(stdout, "%s", details);
    if (details[strlen(details) - 1] != '\n')
      fprintf(stdout, "\n");
    fprintf(stdout, "]\n");
  }
  fflush(stdout);
}

/* these functions all flush to ensure that the test runner knows the action
 * that has been taken even if the subsequent test etc takes a long time or
 * never completes (i.e. a segfault).
 */

void
subunit_test_start(char const * const name)
{
  subunit_send_event("test", name, NULL);
}


void
subunit_test_pass(char const * const name)
{
  /* TODO: add success details as an option */
  subunit_send_event("success", name, NULL);
}


void
subunit_test_fail(char const * const name, char const * const error)
{
  subunit_send_event("failure", name, error);
}


void
subunit_test_error(char const * const name, char const * const error)
{
  subunit_send_event("error", name, error);
}


void
subunit_test_skip(char const * const name, char const * const reason)
{
  subunit_send_event("skip", name, reason);
}

void
subunit_progress(enum subunit_progress_whence whence, int offset)
{
	switch (whence) {
	case SUBUNIT_PROGRESS_SET:
		printf("progress: %d\n", offset);
		break;
	case SUBUNIT_PROGRESS_CUR:
		printf("progress: %+-d\n", offset);
		break;
	case SUBUNIT_PROGRESS_POP:
		printf("progress: pop\n");
		break;
	case SUBUNIT_PROGRESS_PUSH:
		printf("progress: push\n");
		break;
	default:
		fprintf(stderr, "Invalid whence %d in subunit_progress()\n", whence);
		break;
	}
}
