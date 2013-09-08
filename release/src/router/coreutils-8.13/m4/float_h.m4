# float_h.m4 serial 7
dnl Copyright (C) 2007, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FLOAT_H],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])
  FLOAT_H=
  REPLACE_FLOAT_LDBL=0
  case "$host_os" in
    aix* | beos* | openbsd* | mirbsd* | irix*)
      FLOAT_H=float.h
      ;;
    freebsd*)
      case "$host_cpu" in
changequote(,)dnl
        i[34567]86 )
changequote([,])dnl
          FLOAT_H=float.h
          ;;
        x86_64 )
          # On x86_64 systems, the C compiler may still be generating
          # 32-bit code.
          AC_EGREP_CPP([yes],
            [#if defined __LP64__ || defined __x86_64__ || defined __amd64__
             yes
             #endif],
            [],
            [FLOAT_H=float.h])
          ;;
      esac
      ;;
  esac
  case "$host_os" in
    aix* | freebsd*)
      if test -n "$FLOAT_H"; then
        REPLACE_FLOAT_LDBL=1
      fi
      ;;
  esac
  if test -n "$FLOAT_H"; then
    gl_NEXT_HEADERS([float.h])
  fi
  AC_SUBST([FLOAT_H])
  AM_CONDITIONAL([GL_GENERATE_FLOAT_H], [test -n "$FLOAT_H"])
])
