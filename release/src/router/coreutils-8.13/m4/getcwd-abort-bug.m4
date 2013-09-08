# serial 4
# Determine whether getcwd aborts when the length of the working directory
# name is unusually large.  Any length between 4k and 16k trigger the bug
# when using glibc-2.4.90-9 or older.

# Copyright (C) 2006, 2009-2011 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# From Jim Meyering

# gl_FUNC_GETCWD_ABORT_BUG([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
AC_DEFUN([gl_FUNC_GETCWD_ABORT_BUG],
[
  AC_CHECK_DECLS_ONCE([getcwd])
  AC_CHECK_FUNCS([getpagesize])
  AC_CACHE_CHECK([whether getcwd aborts when 4k < cwd_length < 16k],
    gl_cv_func_getcwd_abort_bug,
    [# Remove any remnants of a previous test.
     rm -rf confdir-14B---
     # Arrange for deletion of the temporary directory this test creates.
     ac_clean_files="$ac_clean_files confdir-14B---"
     dnl Please keep this in sync with tests/test-getcwd.c.
     AC_RUN_IFELSE(
       [AC_LANG_SOURCE(
          [[
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

/* Don't get link errors because mkdir is redefined to rpl_mkdir.  */
#undef mkdir

#ifndef S_IRWXU
# define S_IRWXU 0700
#endif

/* FIXME: skip the run-test altogether on systems without getpagesize.  */
#if ! HAVE_GETPAGESIZE
# define getpagesize() 0
#endif

/* This size is chosen to be larger than PATH_MAX (4k), yet smaller than
   the 16kB pagesize on ia64 linux.  Those conditions make the code below
   trigger a bug in glibc's getcwd implementation before 2.4.90-10.  */
#define TARGET_LEN (5 * 1024)

int
main ()
{
  char const *dir_name = "confdir-14B---";
  char *cwd;
  size_t initial_cwd_len;
  int fail = 0;
  size_t desired_depth;
  size_t d;

  /* The bug is triggered when PATH_MAX < getpagesize (), so skip
     this relatively expensive and invasive test if that's not true.  */
  if (getpagesize () <= PATH_MAX)
    return 0;

  cwd = getcwd (NULL, 0);
  if (cwd == NULL)
    return 2;

  initial_cwd_len = strlen (cwd);
  free (cwd);
  desired_depth = ((TARGET_LEN - 1 - initial_cwd_len)
                   / (1 + strlen (dir_name)));
  for (d = 0; d < desired_depth; d++)
    {
      if (mkdir (dir_name, S_IRWXU) < 0 || chdir (dir_name) < 0)
        {
          fail = 3; /* Unable to construct deep hierarchy.  */
          break;
        }
    }

  /* If libc has the bug in question, this invocation of getcwd
     results in a failed assertion.  */
  cwd = getcwd (NULL, 0);
  if (cwd == NULL)
    fail = 4; /* getcwd failed.  This is ok, and expected.  */
  free (cwd);

  /* Call rmdir first, in case the above chdir failed.  */
  rmdir (dir_name);
  while (0 < d--)
    {
      if (chdir ("..") < 0)
        {
          fail = 5;
          break;
        }
      rmdir (dir_name);
    }

  return fail;
}
          ]])],
    [gl_cv_func_getcwd_abort_bug=no],
    dnl A "regular" nonzero return does not indicate this bug.
    dnl An abort will provoke an exit code of something like 134 (128 + 6).
    [test $? -gt 128 \
      && gl_cv_func_getcwd_abort_bug=yes \
      || gl_cv_func_getcwd_abort_bug=no],
    [gl_cv_func_getcwd_abort_bug=yes])
  ])
  AS_IF([test $gl_cv_func_getcwd_abort_bug = yes], [$1], [$2])
])
