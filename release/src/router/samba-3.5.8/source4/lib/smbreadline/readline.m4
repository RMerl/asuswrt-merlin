###############################################
# Readline included by default unless explicitly asked not to
test "${with_readline+set}" != "set" && with_readline=yes

EXTERNAL_READLINE=no
# test for where we get readline() from
AC_MSG_CHECKING(whether to use readline)
AC_ARG_WITH(readline,
[AS_HELP_STRING([--with-readline[=DIR]], [Look for readline include/libs in DIR (default=auto)])],
[  case "$with_readline" in
  yes)
    AC_MSG_RESULT(yes)

    AC_CHECK_HEADERS(readline.h history.h readline/readline.h)
    AC_CHECK_HEADERS(readline/history.h)

    AC_CHECK_HEADERS(readline.h readline/readline.h,[
      for termlib in ncurses curses termcap terminfo termlib tinfo; do
       AC_CHECK_LIB(${termlib}, tgetent, [TERMLIBS="-l${termlib}"; break])
      done
      AC_CHECK_LIB(readline, rl_callback_handler_install,
       [TERMLIBS="-lreadline $TERMLIBS"
       EXTERNAL_READLINE=yes
       break], [TERMLIBS=], $TERMLIBS)])
    ;;
  no)
    AC_MSG_RESULT(no)
    ;;
  *)
    AC_MSG_RESULT(yes)

    # Needed for AC_CHECK_HEADERS and AC_CHECK_LIB to look at
    # alternate readline path
    _ldflags=${LDFLAGS}
    _cppflags=${CPPFLAGS}

    # Add additional search path
    LDFLAGS="-L$with_readline/lib $LDFLAGS"
    CPPFLAGS="-I$with_readline/include $CPPFLAGS"

    AC_CHECK_HEADERS(readline.h history.h readline/readline.h)
    AC_CHECK_HEADERS(readline/history.h)

    AC_CHECK_HEADERS(readline.h readline/readline.h,[
      for termlib in ncurses curses termcap terminfo termlib; do
       AC_CHECK_LIB(${termlib}, tgetent, [TERMLIBS="-l${termlib}"; break])
      done
      AC_CHECK_LIB(readline, rl_callback_handler_install,
       [TERMLDFLAGS="-L$with_readline/lib"
       TERMCPPFLAGS="-I$with_readline/include"
       LDFLAGS="-L$with_readline/lib $LDFLAGS"
       CPPFLAGS="-I$with_readline/include $CPPFLAGS"
       TERMLIBS="-lreadline $TERMLIBS"
       EXTERNAL_READLINE=yes
       break], [TERMLIBS= CPPFLAGS=$_cppflags], $TERMLIBS)])

    ;;
  esac],
  AC_MSG_RESULT(no)
)

# The readline API changed slightly from readline3 to readline4, so
# code will generate warnings on one of them unless we have a few
# special cases.
AC_CHECK_LIB(readline, rl_completion_matches,
	     [AC_DEFINE(HAVE_NEW_LIBREADLINE, 1, 
			[Do we have rl_completion_matches?])],
	     [],
	     [$TERMLIBS])

# not all readline libs have rl_event_hook or history_list
AC_CHECK_DECLS(rl_event_hook, [], [], [
    #include <stdio.h>
    #include <readline/readline.h>
])
AC_CHECK_LIB(readline, history_list,
	     [AC_DEFINE(HAVE_HISTORY_LIST, 1, [Do we have history_list?])],
	     [],
	     [$TERMLIBS])

AC_MSG_CHECKING(whether to use extern readline)
if test x"$EXTERNAL_READLINE" = x"yes"; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBREADLINE,1,[Whether the system has readline])
	SMB_SUBSYSTEM(SMBREADLINE, [\$(smbreadlinesrcdir)/smbreadline.o], [READLINE])
	SMB_EXT_LIB(READLINE, [${TERMLIBS}])
	SMB_ENABLE(READLINE,YES)
else
	SMB_SUBSYSTEM(SMBREADLINE, [\$(smbreadlinesrcdir)/smbreadline.o], [])
	AC_MSG_RESULT(no)
fi
