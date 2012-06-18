dnl
dnl set $(LOCALEDIR) from --with-localedir=value
dnl
define(WITH_LOCALEDIR,[
AC_ARG_WITH([localedir],
[  --with-localedir=PATH   specify locale information directory],
AC_MSG_RESULT(LOCALEDIR is $withval)
LOCALEDIR="$withval",
LOCALEDIR="$LOCALEDIR"
[AC_MSG_RESULT(LOCALEDIR defaults to $LOCALEDIR)]
)dnl
AC_SUBST(LOCALEDIR)dnl
])dnl

define(WITH_CPPOPTS,[
AC_ARG_WITH([ccpopts],
[  --with-ccopts    OBSOLETE - use CPPFLAGS=..., see configure --help],
[AC_MSG_ERROR([ --with-cppopts  OBSOLETE - use CPPFLAGS=... see configure --help])]
)])dnl

define(WITH_LDOPTS,[
AC_ARG_WITH([ldopts],
[  --with-ldopts    OBSOLETE - use LDFLAGS=..., see configure --help],
[AC_MSG_ERROR([ --with-ldopts  OBSOLETE - use LDFLAGS=... see configure --help])]
)])dnl

define(WITH_CCOPTS,[
AC_ARG_WITH([ccopts],
[  --with-copts     OBSOLETE - use CFLAGS=..., see configure --help],
[AC_MSG_ERROR([ --with-ccopts   OBSOLETE - use CFLAGS=... see configure --help])]
)])dnl

