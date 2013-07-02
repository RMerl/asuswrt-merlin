##                                                          -*- Autoconf -*-
# Copyright (C) 2002, 2003, 2005  Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 2

# Obsolete Automake macros.

# We put here only the macros whose substitution is not an Automake
# macro; otherwise including this file would trigger dependencies for
# all the substitutions.  Generally, obsolete Automake macros are
# better AU_DEFUNed in the same file as their replacement, or alone in
# a separate file (see obsol-gt.m4 or obsol-lt.m4 for instance).

AU_DEFUN([AC_FEATURE_CTYPE],     [AC_HEADER_STDC])
AU_DEFUN([AC_FEATURE_ERRNO],     [AC_REPLACE_FUNCS([strerror])])
AU_DEFUN([AM_CYGWIN32],	         [AC_CYGWIN])
AU_DEFUN([AM_EXEEXT],            [AC_EXEEXT])
AU_DEFUN([AM_FUNC_MKTIME],       [AC_FUNC_MKTIME])
AU_DEFUN([AM_HEADER_TIOCGWINSZ_NEEDS_SYS_IOCTL],
				 [AC_HEADER_TIOCGWINSZ])
AU_DEFUN([AM_MINGW32],           [AC_MINGW32])
AU_DEFUN([AM_PROG_INSTALL],      [AC_PROG_INSTALL])
AU_DEFUN([AM_SANITY_CHECK_CC],   [AC_PROG_CC])
AU_DEFUN([AM_SYS_POSIX_TERMIOS], [AC_SYS_POSIX_TERMIOS])
AU_DEFUN([fp_FUNC_FNMATCH],      [AC_FUNC_FNMATCH])
AU_DEFUN([fp_PROG_INSTALL],      [AC_PROG_INSTALL])
AU_DEFUN([md_TYPE_PTRDIFF_T],    [AC_CHECK_TYPES([ptrdiff_t])])

# Don't know how to translate these.
# If used, Autoconf will complain that they are possibly unexpended;
# this seems a good enough error message.
# AC_FEATURE_EXIT
# AC_SYSTEM_HEADER
