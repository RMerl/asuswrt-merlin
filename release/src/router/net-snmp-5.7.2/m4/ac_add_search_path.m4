dnl
dnl Add a search path to the LIBS and CFLAGS variables
dnl
AC_DEFUN([AC_ADD_SEARCH_PATH],[
  if test "x$1" != x -a -d $1; then
     if test -d $1/lib; then
       LDFLAGS="-L$1/lib $LDFLAGS"
     fi
     if test -d $1/include; then
	CPPFLAGS="-I$1/include $CPPFLAGS"
     fi
  fi
])
