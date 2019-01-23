AC_ARG_WITH(included-iniparser,
[AS_HELP_STRING([--with-included-iniparser], [use bundled iniparser library, not from system])],
[
  case "$withval" in
  yes)
    INCLUDED_INIPARSER=yes
    ;;
  no)
    INCLUDED_INIPARSER=no
    ;;
  esac ],
)
if test x"$INCLUDED_INIPARSER" != x"yes"; then
    AC_CHECK_LIB_EXT(iniparser, LIBINIPARSER_LIBS, iniparser_load)

fi

AC_MSG_CHECKING(whether to use included iniparser)
if test x"$ac_cv_lib_ext_iniparser" != x"yes"; then

  iniparserpaths="../iniparser ../lib/iniparser"
  for d in $iniparserpaths; do
    if test -f "$srcdir/$d/src/iniparser.c"; then
      iniparserdir="$d"
      break;
    fi
  done
  if test x"$iniparserdir" = "x"; then
     AC_MSG_ERROR([cannot find iniparser source in $iniparserpaths])
  fi
  INIPARSER_CFLAGS="-I$srcdir/$iniparserdir/src"
  AC_MSG_RESULT(yes)

  INIPARSER_OBJS=""
  INIPARSER_OBJS="$INIPARSER_OBJS $srcdir/$iniparserdir/../iniparser_build/iniparser.o"
  INIPARSER_OBJS="$INIPARSER_OBJS $srcdir/$iniparserdir/../iniparser_build/dictionary.o"
  INIPARSER_OBJS="$INIPARSER_OBJS $srcdir/$iniparserdir/../iniparser_build/strlib.o"

  SMB_SUBSYSTEM(LIBINIPARSER,[$INIPARSER_OBJS],[],[$INIPARSER_CFLAGS])
else
  AC_MSG_RESULT(no)
  SMB_EXT_LIB(LIBINIPARSER,,,,${LIBINIPARSER_LIBS})
  SMB_ENABLE(LIBINIPARSER,YES)
fi

