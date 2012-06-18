########################################################
# Compile with SQLITE3 support?

SQLITE3_LIBS=""
with_sqlite3_support=no
AC_MSG_CHECKING([for SQLITE3 support])

AC_ARG_WITH(sqlite3,
AS_HELP_STRING([--with-sqlite3],[SQLITE3 backend support (default=no)]),
[ case "$withval" in
    yes|no|auto)
	with_sqlite3_support=$withval
	;;
  esac ])

AC_MSG_RESULT($with_sqlite3_support)

if test x"$with_sqlite3_support" != x"no"; then
  ##################################################################
  # first test for sqlite3.h
  AC_CHECK_HEADERS(sqlite3.h)
  
  if test x"$ac_cv_header_sqlite3_h" != x"yes"; then
	if test x"$with_sqlite3_support" = x"yes"; then
	 AC_MSG_ERROR(sqlite3.h is needed for SQLITE3 support)
	else
	 AC_MSG_WARN(sqlite3.h is needed for SQLITE3 support)
	fi
	
	with_sqlite3_support=no
  fi
fi

if test x"$with_sqlite3_support" != x"no"; then
  ac_save_LIBS=$LIBS

  ########################################################
  # now see if we can find the sqlite3 libs in standard paths
  AC_CHECK_LIB_EXT(sqlite3, SQLITE3_LIBS, sqlite3_open)

  if test x"$ac_cv_lib_ext_sqlite3_sqlite3_open" = x"yes"; then
    AC_DEFINE(HAVE_SQLITE3,1,[Whether sqlite3 is available])
    AC_DEFINE(HAVE_LDB_SQLITE3,1,[Whether ldb_sqlite3 is available])
    AC_MSG_CHECKING(whether SQLITE3 support is used)
    AC_MSG_RESULT(yes)
    with_sqlite3_support=yes
    SMB_ENABLE(SQLITE3,YES)
  else
    if test x"$with_sqlite3_support" = x"yes"; then
	AC_MSG_ERROR(libsqlite3 is needed for SQLITE3 support)
    else
	AC_MSG_WARN(libsqlite3 is needed for SQLITE3 support)
    fi

    SQLITE3_LIBS=""
    with_sqlite3_support=no
  fi

  LIBS=$ac_save_LIBS;
fi

SMB_EXT_LIB(SQLITE3,[${SQLITE3_LIBS}],[${SQLITE3_CFLAGS}],[${SQLITE3_CPPFLAGS}],[${SQLITE3_LDFLAGS}])
