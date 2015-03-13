/* Unit testing declarations.
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef TEST_H
#define TEST_H

/* from MinUnit */
#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) \
do { \
  const char *message; \
  puts("RUNNING TEST " #test "..."); \
  message = test(); \
  tests_run++; \
  if (message) return message; \
  puts("PASSED\n"); \
} while (0)

extern int tests_run;

const char *test_parse_content_disposition(void);
const char *test_commands_sorted(void);
const char *test_cmd_spec_restrict_file_names(void);
const char *test_is_robots_txt_url(void);
const char *test_path_simplify (void);
const char *test_append_uri_pathel(void);
const char *test_are_urls_equal(void);
const char *test_subdir_p(void);
const char *test_dir_matches_p(void);

#endif /* TEST_H */

/*
 * vim: et ts=2 sw=2
 */
