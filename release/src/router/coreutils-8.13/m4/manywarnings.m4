# manywarnings.m4 serial 1
dnl Copyright (C) 2008-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Simon Josefsson

# gl_MANYWARN_COMPLEMENT(OUTVAR, LISTVAR, REMOVEVAR)
# --------------------------------------------------
# Copy LISTVAR to OUTVAR except for the entries in REMOVEVAR.
# Elements separated by whitespace.  In set logic terms, the function
# does OUTVAR = LISTVAR \ REMOVEVAR.
AC_DEFUN([gl_MANYWARN_COMPLEMENT],
[
  gl_warn_set=
  set x $2; shift
  for gl_warn_item
  do
    case " $3 " in
      *" $gl_warn_item "*)
        ;;
      *)
        gl_warn_set="$gl_warn_set $gl_warn_item"
        ;;
    esac
  done
  $1=$gl_warn_set
])

# gl_MANYWARN_ALL_GCC(VARIABLE)
# -----------------------------
# Add all documented GCC (currently as per version 4.4) warning
# parameters to variable VARIABLE.  Note that you need to test them
# using gl_WARN_ADD if you want to make sure your gcc understands it.
AC_DEFUN([gl_MANYWARN_ALL_GCC],
[
  dnl First, check if -Wno-missing-field-initializers is needed.
  dnl -Wmissing-field-initializers is implied by -W, but that issues
  dnl warnings with GCC version before 4.7, for the common idiom
  dnl of initializing types on the stack to zero, using { 0, }
  AC_REQUIRE([AC_PROG_CC])
  if test -n "$GCC"; then

    dnl First, check -W -Werror -Wno-missing-field-initializers is supported
    dnl with the current $CC $CFLAGS $CPPFLAGS.
    AC_MSG_CHECKING([whether -Wno-missing-field-initializers is supported])
    AC_CACHE_VAL([gl_cv_cc_nomfi_supported], [
      gl_save_CFLAGS="$CFLAGS"
      CFLAGS="$CFLAGS -W -Werror -Wno-missing-field-initializers"
      AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[]], [[]])],
        [gl_cv_cc_nomfi_supported=yes],
        [gl_cv_cc_nomfi_supported=no])
      CFLAGS="$gl_save_CFLAGS"])
    AC_MSG_RESULT([$gl_cv_cc_nomfi_supported])

    if test "$gl_cv_cc_nomfi_supported" = yes; then
      dnl Now check whether -Wno-missing-field-initializers is needed
      dnl for the { 0, } construct.
      AC_MSG_CHECKING([whether -Wno-missing-field-initializers is needed])
      AC_CACHE_VAL([gl_cv_cc_nomfi_needed], [
        gl_save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS -W -Werror"
        AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM(
             [[void f (void)
               {
                 typedef struct { int a; int b; } s_t;
                 s_t s1 = { 0, };
               }
             ]],
             [[]])],
          [gl_cv_cc_nomfi_needed=no],
          [gl_cv_cc_nomfi_needed=yes])
        CFLAGS="$gl_save_CFLAGS"
      ])
      AC_MSG_RESULT([$gl_cv_cc_nomfi_needed])
    fi
  fi

  gl_manywarn_set=
  for gl_manywarn_item in \
    -Wall \
    -W \
    -Wformat-y2k \
    -Wformat-nonliteral \
    -Wformat-security \
    -Winit-self \
    -Wmissing-include-dirs \
    -Wswitch-default \
    -Wswitch-enum \
    -Wunused \
    -Wunknown-pragmas \
    -Wstrict-aliasing \
    -Wstrict-overflow \
    -Wsystem-headers \
    -Wfloat-equal \
    -Wtraditional \
    -Wtraditional-conversion \
    -Wdeclaration-after-statement \
    -Wundef \
    -Wshadow \
    -Wunsafe-loop-optimizations \
    -Wpointer-arith \
    -Wbad-function-cast \
    -Wc++-compat \
    -Wcast-qual \
    -Wcast-align \
    -Wwrite-strings \
    -Wconversion \
    -Wsign-conversion \
    -Wlogical-op \
    -Waggregate-return \
    -Wstrict-prototypes \
    -Wold-style-definition \
    -Wmissing-prototypes \
    -Wmissing-declarations \
    -Wmissing-noreturn \
    -Wmissing-format-attribute \
    -Wpacked \
    -Wpadded \
    -Wredundant-decls \
    -Wnested-externs \
    -Wunreachable-code \
    -Winline \
    -Winvalid-pch \
    -Wlong-long \
    -Wvla \
    -Wvolatile-register-var \
    -Wdisabled-optimization \
    -Wstack-protector \
    -Woverlength-strings \
    -Wbuiltin-macro-redefined \
    -Wmudflap \
    -Wpacked-bitfield-compat \
    -Wsync-nand \
    ; do
    gl_manywarn_set="$gl_manywarn_set $gl_manywarn_item"
  done
  # The following are not documented in the manual but are included in
  # output from gcc --help=warnings.
  for gl_manywarn_item in \
    -Wattributes \
    -Wcoverage-mismatch \
    -Wmultichar \
    -Wunused-macros \
    ; do
    gl_manywarn_set="$gl_manywarn_set $gl_manywarn_item"
  done

  # Disable the missing-field-initializers warning if needed
  if test "$gl_cv_cc_nomfi_needed" = yes; then
    gl_manywarn_set="$gl_manywarn_set -Wno-missing-field-initializers"
  fi

  $1=$gl_manywarn_set
])
