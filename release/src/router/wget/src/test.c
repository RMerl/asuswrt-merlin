/* Unit testing.
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free
   Software Foundation, Inc.

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

#include "wget.h"

#include <stdio.h>
#ifdef ENABLE_NLS
# include <locale.h>
#endif

#include "test.h"

#ifndef TESTING
#error "TESTING not set!!!"
#endif

const char *program_argstring = "TEST";

static int tests_run;

static const char *
all_tests(void)
{
#ifdef HAVE_METALINK
  mu_run_test (test_find_key_value);
  mu_run_test (test_find_key_values);
  mu_run_test (test_has_key);
#endif
  mu_run_test (test_parse_content_disposition);
  mu_run_test (test_parse_range_header);
  mu_run_test (test_subdir_p);
  mu_run_test (test_dir_matches_p);
  mu_run_test (test_commands_sorted);
  mu_run_test (test_cmd_spec_restrict_file_names);
  mu_run_test (test_path_simplify);
  mu_run_test (test_append_uri_pathel);
  mu_run_test (test_are_urls_equal);
  mu_run_test (test_is_robots_txt_url);
#ifdef HAVE_HSTS
  mu_run_test (test_hsts_new_entry);
  mu_run_test (test_hsts_url_rewrite_superdomain);
  mu_run_test (test_hsts_url_rewrite_congruent);
  mu_run_test (test_hsts_read_database);
#endif

  return NULL;
}

const char *program_name; /* Needed by lib/error.c. */

int
main (int argc _GL_UNUSED, const char *argv[])
{
  const char *result;

  printf ("[DEBUG] Testing...\n\n");
#ifdef ENABLE_NLS
  /* Set the current locale.  */
  setlocale (LC_ALL, "");
  /* Set the text message domain.  */
  bindtextdomain ("wget", LOCALEDIR);
  textdomain ("wget");
#endif /* ENABLE_NLS */

  program_name = argv[0];

  result = all_tests();

  if (result != NULL)
    {
      puts (result);
    }
  else
    {
      printf ("ALL TESTS PASSED\n");
    }

  printf ("Tests run: %d\n", tests_run);

  return result != 0;
}

/*
 * vim: et ts=2 sw=2
 */
