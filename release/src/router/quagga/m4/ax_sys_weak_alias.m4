# ===========================================================================
# http://www.gnu.org/software/autoconf-archive/ax_sys_weak_alias.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_SYS_WEAK_ALIAS
#
# DESCRIPTION
#
#   Determines whether weak aliases are supported on the system, and if so,
#   what scheme is used to declare them. Also checks to see if aliases can
#   cross object file boundaries, as some systems don't permit them to.
#
#   Most systems permit something called a "weak alias" or "weak symbol."
#   These aliases permit a library to provide a stub form of a routine
#   defined in another library, thus allowing the first library to operate
#   even if the other library is not linked. This macro will check for
#   support of weak aliases, figure out what schemes are available, and
#   determine some characteristics of the weak alias support -- primarily,
#   whether a weak alias declared in one object file may be referenced from
#   another object file.
#
#   There are four known schemes of declaring weak symbols; each scheme is
#   checked in turn, and the first one found is prefered. Note that only one
#   of the mentioned preprocessor macros will be defined!
#
#   1. Function attributes
#
#   This scheme was first introduced by the GNU C compiler, and attaches
#   attributes to particular functions. It is among the easiest to use, and
#   so is the first one checked. If this scheme is detected, the
#   preprocessor macro HAVE_SYS_WEAK_ALIAS_ATTRIBUTE will be defined to 1.
#   This scheme is used as in the following code fragment:
#
#     void __weakf(int c)
#     {
#       /* Function definition... */
#     }
#
#     void weakf(int c) __attribute__((weak, alias("__weakf")));
#
#   2. #pragma weak
#
#   This scheme is in use by many compilers other than the GNU C compiler.
#   It is also particularly easy to use, and fairly portable -- well, as
#   portable as these things get. If this scheme is detected first, the
#   preprocessor macro HAVE_SYS_WEAK_ALIAS_PRAGMA will be defined to 1. This
#   scheme is used as in the following code fragment:
#
#     extern void weakf(int c);
#     #pragma weak weakf = __weakf
#     void __weakf(int c)
#     {
#       /* Function definition... */
#     }
#
#   3. #pragma _HP_SECONDARY_DEF
#
#   This scheme appears to be in use by the HP compiler. As it is rather
#   specialized, this is one of the last schemes checked. If it is the first
#   one detected, the preprocessor macro HAVE_SYS_WEAK_ALIAS_HPSECONDARY
#   will be defined to 1. This scheme is used as in the following code
#   fragment:
#
#     extern void weakf(int c);
#     #pragma _HP_SECONDARY_DEF __weakf weakf
#     void __weakf(int c)
#     {
#       /* Function definition... */
#     }
#
#   4. #pragma _CRI duplicate
#
#   This scheme appears to be in use by the Cray compiler. As it is rather
#   specialized, it too is one of the last schemes checked. If it is the
#   first one detected, the preprocessor macro
#   HAVE_SYS_WEAK_ALIAS_CRIDUPLICATE will be defined to 1. This scheme is
#   used as in the following code fragment:
#
#     extern void weakf(int c);
#     #pragma _CRI duplicate weakf as __weakf
#     void __weakf(int c)
#     {
#       /* Function definition... */
#     }
#
#   In addition to the preprocessor macros listed above, if any scheme is
#   found, the preprocessor macro HAVE_SYS_WEAK_ALIAS will also be defined
#   to 1.
#
#   Once a weak aliasing scheme has been found, a check will be performed to
#   see if weak aliases are honored across object file boundaries. If they
#   are, the HAVE_SYS_WEAK_ALIAS_CROSSFILE preprocessor macro is defined to
#   1.
#
#   This Autoconf macro also makes two substitutions. The first, WEAK_ALIAS,
#   contains the name of the scheme found (one of "attribute", "pragma",
#   "hpsecondary", or "criduplicate"), or "no" if no weak aliasing scheme
#   was found. The second, WEAK_ALIAS_CROSSFILE, is set to "yes" or "no"
#   depending on whether or not weak aliases may cross object file
#   boundaries.
#
# LICENSE
#
#   Copyright (c) 2008 Kevin L. Mitchell <klmitch@mit.edu>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6

AU_ALIAS([KLM_SYS_WEAK_ALIAS], [AX_SYS_WEAK_ALIAS])
AC_DEFUN([AX_SYS_WEAK_ALIAS], [
  # starting point: no aliasing scheme yet...
  ax_sys_weak_alias=no

  # Figure out what kind of aliasing may be supported...
  _AX_SYS_WEAK_ALIAS_ATTRIBUTE
  _AX_SYS_WEAK_ALIAS_PRAGMA
  _AX_SYS_WEAK_ALIAS_HPSECONDARY
  _AX_SYS_WEAK_ALIAS_CRIDUPLICATE

  # Do we actually support aliasing?
  AC_CACHE_CHECK([how to create weak aliases with $CC],
                 [ax_cv_sys_weak_alias],
 [ax_cv_sys_weak_alias=$ax_sys_weak_alias])

  # OK, set a #define
  AS_IF([test $ax_cv_sys_weak_alias != no], [
    AC_DEFINE([HAVE_SYS_WEAK_ALIAS], 1,
              [Define this if your system can create weak aliases])
  ])

  # Can aliases cross object file boundaries?
  _AX_SYS_WEAK_ALIAS_CROSSFILE

  # OK, remember the results
  AC_SUBST([WEAK_ALIAS], [$ax_cv_sys_weak_alias])
  AC_SUBST([WEAK_ALIAS_CROSSFILE], [$ax_cv_sys_weak_alias_crossfile])
])

AC_DEFUN([_AX_SYS_WEAK_ALIAS_ATTRIBUTE],
[ # Test whether compiler accepts __attribute__ form of weak aliasing
  AC_CACHE_CHECK([whether $CC accepts function __attribute__((weak,alias()))],
  [ax_cv_sys_weak_alias_attribute], [
    # We add -Werror if it's gcc to force an error exit if the weak attribute
    # isn't understood
    AS_IF([test $GCC = yes], [
      save_CFLAGS=$CFLAGS
      CFLAGS=-Werror])

    # Try linking with a weak alias...
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([
void __weakf(int c) {}
void weakf(int c) __attribute__((weak, alias("__weakf")));],
        [weakf(0)])],
      [ax_cv_sys_weak_alias_attribute=yes],
      [ax_cv_sys_weak_alias_attribute=no])

    # Restore original CFLAGS
    AS_IF([test $GCC = yes], [
      CFLAGS=$save_CFLAGS])
  ])

  # What was the result of the test?
  AS_IF([test $ax_cv_sys_weak_alias_attribute = yes], [
    test $ax_sys_weak_alias = no && ax_sys_weak_alias=attribute
    AC_DEFINE([HAVE_SYS_WEAK_ALIAS_ATTRIBUTE], 1,
              [Define this if weak aliases may be created with __attribute__])
  ])
])

AC_DEFUN([_AX_SYS_WEAK_ALIAS_PRAGMA],
[ # Test whether compiler accepts #pragma form of weak aliasing
  AC_CACHE_CHECK([whether $CC supports @%:@pragma weak],
  [ax_cv_sys_weak_alias_pragma], [

    # Try linking with a weak alias...
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([
extern void weakf(int c);
@%:@pragma weak weakf = __weakf
void __weakf(int c) {}],
        [weakf(0)])],
      [ax_cv_sys_weak_alias_pragma=yes],
      [ax_cv_sys_weak_alias_pragma=no])
  ])

  # What was the result of the test?
  AS_IF([test $ax_cv_sys_weak_alias_pragma = yes], [
    test $ax_sys_weak_alias = no && ax_sys_weak_alias=pragma
    AC_DEFINE([HAVE_SYS_WEAK_ALIAS_PRAGMA], 1,
              [Define this if weak aliases may be created with @%:@pragma weak])
  ])
])

AC_DEFUN([_AX_SYS_WEAK_ALIAS_HPSECONDARY],
[ # Test whether compiler accepts _HP_SECONDARY_DEF pragma from HP...
  AC_CACHE_CHECK([whether $CC supports @%:@pragma _HP_SECONDARY_DEF],
  [ax_cv_sys_weak_alias_hpsecondary], [

    # Try linking with a weak alias...
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([
extern void weakf(int c);
@%:@pragma _HP_SECONDARY_DEF __weakf weakf
void __weakf(int c) {}],
        [weakf(0)])],
      [ax_cv_sys_weak_alias_hpsecondary=yes],
      [ax_cv_sys_weak_alias_hpsecondary=no])
  ])

  # What was the result of the test?
  AS_IF([test $ax_cv_sys_weak_alias_hpsecondary = yes], [
    test $ax_sys_weak_alias = no && ax_sys_weak_alias=hpsecondary
    AC_DEFINE([HAVE_SYS_WEAK_ALIAS_HPSECONDARY], 1,
              [Define this if weak aliases may be created with @%:@pragma _HP_SECONDARY_DEF])
  ])
])

AC_DEFUN([_AX_SYS_WEAK_ALIAS_CRIDUPLICATE],
[ # Test whether compiler accepts "_CRI duplicate" pragma from Cray
  AC_CACHE_CHECK([whether $CC supports @%:@pragma _CRI duplicate],
  [ax_cv_sys_weak_alias_criduplicate], [

    # Try linking with a weak alias...
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([
extern void weakf(int c);
@%:@pragma _CRI duplicate weakf as __weakf
void __weakf(int c) {}],
        [weakf(0)])],
      [ax_cv_sys_weak_alias_criduplicate=yes],
      [ax_cv_sys_weak_alias_criduplicate=no])
  ])

  # What was the result of the test?
  AS_IF([test $ax_cv_sys_weak_alias_criduplicate = yes], [
    test $ax_sys_weak_alias = no && ax_sys_weak_alias=criduplicate
    AC_DEFINE([HAVE_SYS_WEAK_ALIAS_CRIDUPLICATE], 1,
              [Define this if weak aliases may be created with @%:@pragma _CRI duplicate])
  ])
])

dnl Note: This macro is modeled closely on AC_LINK_IFELSE, and in fact
dnl depends on some implementation details of that macro, particularly
dnl its use of _AC_MSG_LOG_CONFTEST to log the failed test program and
dnl its use of ac_link for running the linker.
AC_DEFUN([_AX_SYS_WEAK_ALIAS_CROSSFILE],
[ # Check to see if weak aliases can cross object file boundaries
  AC_CACHE_CHECK([whether $CC supports weak aliases across object file boundaries],
  [ax_cv_sys_weak_alias_crossfile], [
    AS_IF([test $ax_cv_sys_weak_alias = no],
          [ax_cv_sys_weak_alias_crossfile=no], [
dnl Must build our own test files...
      # conftest1 contains our weak alias definition...
      cat >conftest1.$ac_ext <<_ACEOF
/* confdefs.h.  */
_ACEOF
      cat confdefs.h >>conftest1.$ac_ext
      cat >>conftest1.$ac_ext <<_ACEOF
/* end confdefs.h.  */

@%:@ifndef HAVE_SYS_WEAK_ALIAS_ATTRIBUTE
extern void weakf(int c);
@%:@if defined(HAVE_SYS_WEAK_ALIAS_PRAGMA)
@%:@pragma weak weakf = __weakf
@%:@elif defined(HAVE_SYS_WEAK_ALIAS_HPSECONDARY)
@%:@pragma _HP_SECONDARY_DEF __weakf weakf
@%:@elif defined(HAVE_SYS_WEAK_ALIAS_CRIDUPLICATE)
@%:@pragma _CRI duplicate weakf as __weakf
@%:@endif
@%:@endif
void __weakf(int c) {}
@%:@ifdef HAVE_SYS_WEAK_ALIAS_ATTRIBUTE
void weakf(int c) __attribute((weak, alias("__weakf")));
@%:@endif
_ACEOF
      # And conftest2 contains our main routine that calls it
      cat >conftest2.$ac_ext <<_ACEOF
/* confdefs.h.  */
_ACEOF
      cat confdefs.h >> conftest2.$ac_ext
      cat >>conftest2.$ac_ext <<_ACEOF
/* end confdefs.h.  */

extern void weakf(int c);
int
main ()
{
  weakf(0);
  return 0;
}
_ACEOF
      # We must remove the object files (if any) ourselves...
      rm -f conftest2.$ac_objext conftest$ac_exeext

      # Change ac_link to compile *2* files together
      save_aclink=$ac_link
      ac_link=`echo "$ac_link" | \
               sed -e 's/conftest\(\.\$ac_ext\)/conftest1\1 conftest2\1/'`
dnl Substitute our own routine for logging the conftest
m4_pushdef([_AC_MSG_LOG_CONFTEST],
[echo "$as_me: failed program was:" >&AS_MESSAGE_LOG_FD
echo ">>> conftest1.$ac_ext" >&AS_MESSAGE_LOG_FD
sed "s/^/| /" conftest1.$ac_ext >&AS_MESSAGE_LOG_FD
echo ">>> conftest2.$ac_ext" >&AS_MESSAGE_LOG_FD
sed "s/^/| /" conftest2.$ac_ext >&AS_MESSAGE_LOG_FD
])dnl
      # Since we created the files ourselves, don't use SOURCE argument
      AC_LINK_IFELSE(, [ax_cv_sys_weak_alias_crossfile=yes],
                     [ax_cv_sys_weak_alias_crossfile=no])
dnl Restore _AC_MSG_LOG_CONFTEST
m4_popdef([_AC_MSG_LOG_CONFTEST])dnl
      # Restore ac_link
      ac_link=$save_aclink

      # We must remove the object files (if any) and C files ourselves...
      rm -f conftest1.$ac_ext conftest2.$ac_ext \
            conftest1.$ac_objext conftest2.$ac_objext
    ])
  ])

  # What were the results of the test?
  AS_IF([test $ax_cv_sys_weak_alias_crossfile = yes], [
    AC_DEFINE([HAVE_SYS_WEAK_ALIAS_CROSSFILE], 1,
              [Define this if weak aliases in other files are honored])
  ])
])
