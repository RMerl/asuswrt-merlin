## ------------------------                                 -*- Autoconf -*-
## Emacs LISP file handling
## From Ulrich Drepper
## Almost entirely rewritten by Alexandre Oliva
## ------------------------
# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
# 2006  Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 10

# AM_PATH_LISPDIR
# ---------------
AC_DEFUN([AM_PATH_LISPDIR],
[AC_PREREQ([2.60])dnl
 # If set to t, that means we are running in a shell under Emacs.
 # If you have an Emacs named "t", then use the full path.
 test x"$EMACS" = xt && EMACS=
 AC_CHECK_PROGS([EMACS], [emacs xemacs], [no])
 AC_ARG_VAR([EMACS], [the Emacs editor command])
 AC_ARG_VAR([EMACSLOADPATH], [the Emacs library search path])
 AC_ARG_WITH([lispdir],
 [  --with-lispdir          override the default lisp directory],
 [ lispdir="$withval"
   AC_MSG_CHECKING([where .elc files should go])
   AC_MSG_RESULT([$lispdir])],
 [
 AC_CACHE_CHECK([where .elc files should go], [am_cv_lispdir], [
   if test $EMACS != "no"; then
     if test x${lispdir+set} != xset; then
  # If $EMACS isn't GNU Emacs or XEmacs, this can blow up pretty badly
  # Some emacsen will start up in interactive mode, requiring C-x C-c to exit,
  #  which is non-obvious for non-emacs users.
  # Redirecting /dev/null should help a bit; pity we can't detect "broken"
  #  emacsen earlier and avoid running this altogether.
  AC_RUN_LOG([$EMACS -batch -q -eval '(while load-path (princ (concat (car load-path) "\n")) (setq load-path (cdr load-path)))' </dev/null >conftest.out])
	am_cv_lispdir=`sed -n \
       -e 's,/$,,' \
       -e '/.*\/lib\/x*emacs\/site-lisp$/{s,.*/lib/\(x*emacs/site-lisp\)$,${libdir}/\1,;p;q;}' \
       -e '/.*\/share\/x*emacs\/site-lisp$/{s,.*/share/\(x*emacs/site-lisp\),${datarootdir}/\1,;p;q;}' \
       conftest.out`
       rm conftest.out
     fi
   fi
   test -z "$am_cv_lispdir" && am_cv_lispdir='${datadir}/emacs/site-lisp'
  ])
  lispdir="$am_cv_lispdir"
])
AC_SUBST([lispdir])
])# AM_PATH_LISPDIR

AU_DEFUN([ud_PATH_LISPDIR], [AM_PATH_LISPDIR])
