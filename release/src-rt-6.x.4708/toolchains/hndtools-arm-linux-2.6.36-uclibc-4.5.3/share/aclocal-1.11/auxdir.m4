# AM_AUX_DIR_EXPAND                                         -*- Autoconf -*-

# Copyright (C) 2001, 2003, 2005  Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# For projects using AC_CONFIG_AUX_DIR([foo]), Autoconf sets
# $ac_aux_dir to `$srcdir/foo'.  In other projects, it is set to
# `$srcdir', `$srcdir/..', or `$srcdir/../..'.
#
# Of course, Automake must honor this variable whenever it calls a
# tool from the auxiliary directory.  The problem is that $srcdir (and
# therefore $ac_aux_dir as well) can be either absolute or relative,
# depending on how configure is run.  This is pretty annoying, since
# it makes $ac_aux_dir quite unusable in subdirectories: in the top
# source directory, any form will work fine, but in subdirectories a
# relative path needs to be adjusted first.
#
# $ac_aux_dir/missing
#    fails when called from a subdirectory if $ac_aux_dir is relative
# $top_srcdir/$ac_aux_dir/missing
#    fails if $ac_aux_dir is absolute,
#    fails when called from a subdirectory in a VPATH build with
#          a relative $ac_aux_dir
#
# The reason of the latter failure is that $top_srcdir and $ac_aux_dir
# are both prefixed by $srcdir.  In an in-source build this is usually
# harmless because $srcdir is `.', but things will broke when you
# start a VPATH build or use an absolute $srcdir.
#
# So we could use something similar to $top_srcdir/$ac_aux_dir/missing,
# iff we strip the leading $srcdir from $ac_aux_dir.  That would be:
#   am_aux_dir='\$(top_srcdir)/'`expr "$ac_aux_dir" : "$srcdir//*\(.*\)"`
# and then we would define $MISSING as
#   MISSING="\${SHELL} $am_aux_dir/missing"
# This will work as long as MISSING is not called from configure, because
# unfortunately $(top_srcdir) has no meaning in configure.
# However there are other variables, like CC, which are often used in
# configure, and could therefore not use this "fixed" $ac_aux_dir.
#
# Another solution, used here, is to always expand $ac_aux_dir to an
# absolute PATH.  The drawback is that using absolute paths prevent a
# configured tree to be moved without reconfiguration.

AC_DEFUN([AM_AUX_DIR_EXPAND],
[dnl Rely on autoconf to set up CDPATH properly.
AC_PREREQ([2.50])dnl
# expand $ac_aux_dir to an absolute path
am_aux_dir=`cd $ac_aux_dir && pwd`
])
