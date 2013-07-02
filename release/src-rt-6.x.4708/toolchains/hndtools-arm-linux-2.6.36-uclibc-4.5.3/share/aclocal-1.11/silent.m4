##                                                          -*- Autoconf -*-
# Copyright (C) 2009  Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 1

# AM_SILENT_RULES([DEFAULT])
# --------------------------
# Enable less verbose build rules; with the default set to DEFAULT
# (`yes' being less verbose, `no' or empty being verbose).
AC_DEFUN([AM_SILENT_RULES],
[AC_ARG_ENABLE([silent-rules],
[  --enable-silent-rules          less verbose build output (undo: `make V=1')
  --disable-silent-rules         verbose build output (undo: `make V=0')])
case $enable_silent_rules in
yes) AM_DEFAULT_VERBOSITY=0;;
no)  AM_DEFAULT_VERBOSITY=1;;
*)   AM_DEFAULT_VERBOSITY=m4_if([$1], [yes], [0], [1]);;
esac
AC_SUBST([AM_DEFAULT_VERBOSITY])dnl
AM_BACKSLASH='\'
AC_SUBST([AM_BACKSLASH])dnl
_AM_SUBST_NOTMAKE([AM_BACKSLASH])dnl
])
