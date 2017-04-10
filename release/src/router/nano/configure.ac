# Configuration for GNU nano - a small and user-friendly text editor
#
# Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,
# 2008, 2009, 2010, 2011, 2013, 2014 Free Software Foundation, Inc.
#
# GNU nano is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# GNU nano is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see http://www.gnu.org/licenses/.

AC_INIT([GNU nano], [2.8.0], [nano-devel@gnu.org], [nano])
AC_CONFIG_SRCDIR([src/nano.c])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.14])
AC_CONFIG_HEADERS([config.h])

dnl Make sure the ONCE macros are available.

AC_PREREQ([2.69])

dnl Lie about gnulib features we don't use to speed up & shrink down.

gl_cv_func_printf_directive_n=yes
gl_cv_func_printf_infinite_long_double=yes
gl_cv_func_printf_long_double=yes
gl_cv_func_snprintf_directive_n=yes
gl_cv_glob_lists_symlinks=yes

dnl Checks for programs.

AC_GNU_SOURCE
AC_PROG_CC
gl_EARLY
gl_INIT
AC_PROG_LN_S
AC_ISC_POSIX
AC_SYS_LARGEFILE
PKG_PROG_PKG_CONFIG

dnl Internationalization macros.

AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external], [need-ngettext])
AM_CONDITIONAL(USE_NLS, test x$USE_NLS = xyes)

dnl Data location.

pkgdatadir=${datadir}/${PACKAGE}
AC_DEFINE_DIR([PKGDATADIR], [pkgdatadir], [Where data are placed to.])

dnl Checks for header files.

AC_CHECK_HEADERS(libintl.h limits.h pwd.h termios.h sys/param.h)

dnl Checks for options.

AC_ARG_ENABLE(browser,
AS_HELP_STRING([--disable-browser], [Disable built-in file browser]))
if test "x$enable_browser" = xno; then
    AC_DEFINE(DISABLE_BROWSER, 1, [Define this to disable the built-in file browser.])
fi

AC_ARG_ENABLE(color,
AS_HELP_STRING([--disable-color], [Disable color and syntax highlighting]))
if test "x$enable_nanorc" = xno; then
    if test "x$enable_color" = xyes; then
	AC_MSG_ERROR([--enable-color cannot work with --disable-nanorc])
    else
	# Disabling nanorc silently disables color support.
	enable_color=no
    fi
fi
if test "x$enable_color" = xno; then
    AC_DEFINE(DISABLE_COLOR, 1, [Define this to disable syntax highlighting.])
else
    color_support=yes
fi

AC_ARG_ENABLE(comment,
AS_HELP_STRING([--disable-comment], [Disable comment/uncomment functions]))
if test "x$enable_tiny" = xyes; then
    if test "x$enable_comment" = xyes; then
	AC_MSG_ERROR([--enable-comment cannot work with --enable-tiny])
    else
	enable_comment=no
    fi
fi
if test "x$disable_comment" != xyes; then
    if test "x$enable_comment" != xno; then
	AC_DEFINE(ENABLE_COMMENT, 1, [Define this to enable the comment/uncomment function.])
    fi
fi

AC_ARG_ENABLE(extra,
AS_HELP_STRING([--disable-extra], [Disable extra features, currently only easter eggs]))
if test "x$enable_extra" = xno; then
    AC_DEFINE(DISABLE_EXTRA, 1, [Define this to disable extra stuff.])
fi

AC_ARG_ENABLE(help,
AS_HELP_STRING([--disable-help], [Disable help functions]))
if test "x$enable_help" = xno; then
    AC_DEFINE(DISABLE_HELP, 1, [Define this to disable the help text display.])
fi

AC_ARG_ENABLE(histories,
AS_HELP_STRING([--disable-histories], [Disable search and position histories]))
if test "x$enable_histories" = xno; then
    AC_DEFINE(DISABLE_HISTORIES, 1, [Define this to disable search and position histories.])
fi

AC_ARG_ENABLE(justify,
AS_HELP_STRING([--disable-justify], [Disable justify/unjustify functions]))
if test "x$enable_justify" = xno; then
    AC_DEFINE(DISABLE_JUSTIFY, 1, [Define this to disable the justify routines.])
fi

AC_ARG_ENABLE(libmagic,
AS_HELP_STRING([--disable-libmagic], [Disable detection of file types via libmagic]))

AC_ARG_ENABLE(linenumbers,
AS_HELP_STRING([--disable-linenumbers], [Disable line numbering]))
if test "x$enable_tiny" = xyes; then
    if test "x$enable_linenumbers" != xyes; then
	enable_linenumbers=no
    fi
fi
if test "x$disable_linenumbers" != xyes; then
    if test "x$enable_linenumbers" != xno; then
	AC_DEFINE(ENABLE_LINENUMBERS, 1, [Define this to enable line numbering.])
    fi
fi

AC_ARG_ENABLE(mouse,
AS_HELP_STRING([--disable-mouse], [Disable mouse support (and -m flag)]))
if test "x$enable_mouse" = xno; then
    AC_DEFINE(DISABLE_MOUSE, 1, [Define this to disable the mouse support.])
fi

AC_ARG_ENABLE(multibuffer,
AS_HELP_STRING([--disable-multibuffer], [Disable multiple file buffers]))
if test "x$enable_multibuffer" = xno; then
    AC_DEFINE(DISABLE_MULTIBUFFER, 1, [Define this to disable multiple file buffers.])
fi

AC_ARG_ENABLE(nanorc,
AS_HELP_STRING([--disable-nanorc], [Disable use of .nanorc files]))
if test "x$enable_nanorc" = xno; then
    AC_DEFINE(DISABLE_NANORC, 1, [Define this to disable the use of .nanorc files.])
else
    nanorc_support=yes
fi

AC_ARG_ENABLE(operatingdir,
AS_HELP_STRING([--disable-operatingdir], [Disable setting of operating directory (chroot of sorts)]))
if test "x$enable_operatingdir" = xno; then
    AC_DEFINE(DISABLE_OPERATINGDIR, 1, [Define this to disable the setting of the operating directory (chroot of sorts).])
fi

AC_ARG_ENABLE(speller,
AS_HELP_STRING([--disable-speller], [Disable spell checker functions]))
if test "x$enable_speller" = xno; then
    AC_DEFINE(DISABLE_SPELLER, 1, [Define this to disable the spell checker functions.])
fi

AC_ARG_ENABLE(tabcomp,
AS_HELP_STRING([--disable-tabcomp], [Disable tab completion functions]))
if test "x$enable_tabcomp" = xno; then
    AC_DEFINE(DISABLE_TABCOMP, 1, [Define this to disable the tab completion functions for filenames and search strings.])
fi

AC_ARG_ENABLE(wordcomp,
AS_HELP_STRING([--disable-wordcomp], [Disable the word completion function]))
if test "x$enable_tiny" = xyes; then
    if test "x$enable_wordcomp" = xyes; then
	AC_MSG_ERROR([--enable-wordcomp cannot work with --enable-tiny])
    else
	enable_wordcomp=no
    fi
fi
if test "x$disable_wordcomp" != xyes; then
    if test "x$enable_wordcomp" != xno; then
	AC_DEFINE(ENABLE_WORDCOMPLETION, 1, [Define this to enable the word completion function.])
    fi
fi

AC_ARG_ENABLE(wrapping,
AS_HELP_STRING([--disable-wrapping], [Disable all wrapping of text (and -w flag)]))
if test "x$enable_wrapping" = xno; then
    AC_DEFINE(DISABLE_WRAPPING, 1, [Define this to disable all text wrapping.])
fi

AC_ARG_ENABLE(wrapping-as-root,
AS_HELP_STRING([--disable-wrapping-as-root], [Disable wrapping of text as root by default]))
if test "x$enable_wrapping_as_root" = xno; then
    AC_DEFINE(DISABLE_ROOTWRAPPING, 1, [Define this to disable text wrapping as root by default.])
fi

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug], [Enable debugging (disabled by default)]))
if test "x$enable_debug" = xyes; then
    AC_DEFINE(DEBUG, 1, [Define this to enable debug messages and assert warnings.])
else
    AC_DEFINE(NDEBUG, 1, [Shut up assert warnings :-)])
fi

AC_ARG_ENABLE(tiny,
AS_HELP_STRING([--enable-tiny], [Disable features for the sake of size]))
if test "x$enable_tiny" = xyes; then
    AC_DEFINE(NANO_TINY, 1, [Define this to make the nano executable as small as possible.])
    if test "x$enable_browser" != xyes; then
	AC_DEFINE(DISABLE_BROWSER, 1, [Define this to disable the built-in file browser.])
    fi
    if test "x$enable_color" != xyes; then
	AC_DEFINE(DISABLE_COLOR, 1, [Define this to disable syntax highlighting.])
	color_support=no
    else
	if test "x$enable_nanorc" != xyes; then
	    AC_MSG_ERROR([--enable-color with --enable-tiny cannot work without --enable-nanorc])
	fi
    fi
    if test "x$enable_extra" != xyes; then
	AC_DEFINE(DISABLE_EXTRA, 1, [Define this to disable extra stuff.])
    fi
    if test "x$enable_help" != xyes; then
	AC_DEFINE(DISABLE_HELP, 1, [Define this to disable the help text display.])
    fi
    if test "x$enable_histories" != xyes; then
	AC_DEFINE(DISABLE_HISTORIES, 1, [Define this to disable search and position histories.])
    fi
    if test "x$enable_justify" != xyes; then
	AC_DEFINE(DISABLE_JUSTIFY, 1, [Define this to disable the justify routines.])
    fi
    if test "x$enable_libmagic" != xyes; then
	enable_libmagic=no
    fi
    if test "x$enable_mouse" != xyes; then
	AC_DEFINE(DISABLE_MOUSE, 1, [Define this to disable the mouse support.])
    fi
    if test "x$enable_multibuffer" != xyes; then
	AC_DEFINE(DISABLE_MULTIBUFFER, 1, [Define this to disable multiple file buffers.])
    fi
    if test "x$enable_nanorc" != xyes; then
	AC_DEFINE(DISABLE_NANORC, 1, [Define this to disable the use of .nanorc files.])
    fi
    if test "x$enable_operatingdir" != xyes; then
	AC_DEFINE(DISABLE_OPERATINGDIR, 1, [Define this to disable the setting of the operating directory (chroot of sorts).])
    fi
    if test "x$enable_speller" != xyes; then
	AC_DEFINE(DISABLE_SPELLER, 1, [Define this to disable the spell checker functions.])
    fi
    if test "x$enable_tabcomp" != xyes; then
	AC_DEFINE(DISABLE_TABCOMP, 1, [Define this to disable the tab completion functions for files and search strings.])
    fi
    if test "x$enable_wrapping" != xyes; then
	AC_DEFINE(DISABLE_WRAPPING, 1, [Define this to disable all text wrapping.])
    fi
fi

AM_CONDITIONAL(USE_COLOR, test x$color_support = xyes)
AM_CONDITIONAL(USE_NANORC, test x$nanorc_support = xyes)

AC_MSG_CHECKING([whether to enable UTF-8 support])
AC_ARG_ENABLE(utf8, AS_HELP_STRING([--enable-utf8], [Enable UTF-8 support]))
AC_MSG_RESULT(${enable_utf8:-auto})

AC_ARG_ENABLE(altrcname,
AS_HELP_STRING([--enable-altrcname], [Specify an alternate rcfile name (default: .nanorc)]),
[if test x$enableval != no; then
    AC_DEFINE_UNQUOTED(RCFILE_NAME, "$enableval", [Specify an alternate rcfile name (default: .nanorc).]) rcfilename=$enableval
fi])


AC_MSG_CHECKING([whether to use slang])
CURSES_LIB_NAME=""
AC_ARG_WITH(slang,
AS_HELP_STRING([--with-slang[=DIR]], [Use the slang library instead of curses]),
[    case "$with_slang" in
    no)
	AC_MSG_RESULT(no)
	;;
    *)
	AC_MSG_RESULT(yes)

	if test x$with_slang != xyes; then
	    # Add additional search path.
	    LDFLAGS="-L$with_slang/lib $LDFLAGS"
	    CPPFLAGS="-I$with_slang/include $CPPFLAGS"
	fi

	if test "x$enable_utf8" != xno; then
	    AC_CHECK_HEADER(slcurses.h,
		AC_MSG_CHECKING([for SLutf8_enable in -lslang])
		_libs=$LIBS
		LIBS="$LIBS -lslang"
		AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLutf8_enable(1);
    return 0;
}],
		    [AC_MSG_RESULT(yes)
		    AC_DEFINE(USE_SLANG, 1, [Define this to use the slang wrappers for curses instead of native curses.])
		    CURSES_LIB_WIDE=yes
		    if test x$with_slang != xyes; then
			CURSES_LIB="-L${with_slang}/lib -lslang"
		    else
			CURSES_LIB="-lslang"
		    fi
		    CURSES_LIB_NAME=slang],
		    [AC_MSG_RESULT(no)

		    # We might need the term library.
		    for termlib in ncurses curses termcap terminfo termlib; do
			AC_CHECK_LIB([${termlib}], tputs, [tcap="-l$termlib"])
			test -n "$tcap" && break
		    done

		    AC_MSG_CHECKING([for SLutf8_enable in -lslang $tcap])
		    LIBS="$LIBS $tcap"
		    AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLutf8_enable(1);
    return 0;
}],
			[AC_MSG_RESULT(yes)
			AC_DEFINE(USE_SLANG, 1, [Define this to use the slang wrappers for curses instead of native curses.])
			CURSES_LIB_WIDE=yes
			if test x$with_slang != xyes; then
			    CURSES_LIB="-L${with_slang}/lib -lslang $tcap"
			else
			    CURSES_LIB="-lslang $tcap"
			fi
			CURSES_LIB_NAME=slang],
			[AC_MSG_RESULT(no)

			# We might need the math library.
			AC_MSG_CHECKING([for SLutf8_enable in -lslang $tcap -lm])
			LIBS="$LIBS -lm"
			AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLutf8_enable(1);
    return 0;
}],
			    [AC_MSG_RESULT(yes)
			    AC_DEFINE(USE_SLANG, 1, [Define this to use the slang wrappers for curses instead of native curses.])
			    CURSES_LIB_WIDE=yes
			    if test x$with_slang != xyes; then
				CURSES_LIB="-L${with_slang}/lib -lslang $tcap -lm"
			    else
				CURSES_LIB="-lslang $tcap -lm"
			    fi
			    CURSES_LIB_NAME=slang],
			    [AC_MSG_RESULT(no)],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling])),
	    AC_MSG_ERROR([
*** The header file slcurses.h was not found.  If you wish to use
*** slang support this header file is required.  Please either
*** install a version of slang that includes the slcurses.h file or
*** do not call the configure script with --with-slang.]))
	fi

	if eval "test x$CURSES_LIB_NAME = x"; then
	    # Reset libs if the above slang tests failed.
	    if test "x$enable_utf8" != xno; then
		LIBS=$_libs
	    fi

	    AC_CHECK_HEADER(slcurses.h,
		AC_MSG_CHECKING([for SLtt_initialize in -lslang])
		_libs=$LIBS
		LIBS="$LIBS -lslang"
		AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLtt_initialize(NULL);
    return 0;
}],
		    [AC_MSG_RESULT(yes)
		    AC_DEFINE(USE_SLANG, 1, [Define this to use the slang wrappers for curses instead of native curses.])
		    if test x$with_slang != xyes; then
			CURSES_LIB="-L${with_slang}/lib -lslang"
		    else
			CURSES_LIB="-lslang"
		    fi
		    CURSES_LIB_NAME=slang],
		    [AC_MSG_RESULT(no)

		    # We might need the term library.
		    for termlib in ncurses curses termcap terminfo termlib; do
			AC_CHECK_LIB([${termlib}], tputs, [tcap="-l$termlib"])
			test -n "$tcap" && break
		    done

		    AC_MSG_CHECKING([for SLtt_initialize in -lslang $tcap])
		    LIBS="$LIBS $tcap"
		    AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLtt_initialize(NULL);
    return 0;
}],
			[AC_MSG_RESULT(yes)
			AC_DEFINE(USE_SLANG, 1, [Define this to use the slang wrappers for curses instead of native curses.])
			if test x$with_slang != xyes; then
			    CURSES_LIB="-L${with_slang}/lib -lslang $tcap"
			else
			    CURSES_LIB="-lslang $tcap"
			fi
			CURSES_LIB_NAME=slang],
			[AC_MSG_RESULT(no)

			# We might need the math library.
			AC_MSG_CHECKING([for SLtt_initialize in -lslang $tcap -lm])
			LIBS="$LIBS -lm"
			AC_TRY_RUN([
#include <slcurses.h>
int main(void)
{
    SLtt_initialize(NULL);
    return 0;
}],
			    [AC_MSG_RESULT(yes)
			    AC_DEFINE(USE_SLANG, 1, [Define this to use the slang wrappers for curses instead of native curses.])
			    if test x$with_slang != xyes; then
				CURSES_LIB="-L${with_slang}/lib -lslang $tcap -lm"
			    else
				CURSES_LIB="-lslang $tcap -lm"
			    fi
			    CURSES_LIB_NAME=slang],
			    [AC_MSG_RESULT(no)],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
    AC_MSG_WARN([*** Can not use slang when cross-compiling])),
	    AC_MSG_ERROR([
*** The header file slcurses.h was not found.  If you wish to use
*** slang support this header file is required.  Please either
*** install a version of slang that includes the slcurses.h file or
*** do not call the configure script with --with-slang.]))
	fi

	if test "${_libs+set}" = "set"; then
	    LIBS=$_libs
	fi

	if test x$with_slang != xyes; then
	    LDFLAGS=${_ldflags}
	fi
	;;
    esac], [AC_MSG_RESULT(no)])


dnl Checks for functions.

if test "x$enable_utf8" != xno; then
    AC_CHECK_FUNCS(iswalnum iswpunct mblen mbstowcs mbtowc wctomb)
fi
dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for available flags.

AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"], [], [])

dnl Checks for library functions.

AC_TYPE_SIGNAL

dnl Checks for libraries.

if eval "test x$CURSES_LIB_NAME = x"; then
    if test "x$enable_utf8" != xno; then
	PKG_CHECK_MODULES([NCURSESW], [ncursesw], [
	    CURSES_LIB=$NCURSESW_LIBS
	    CPPFLAGS="$NCURSESW_CFLAGS $CPPFLAGS"
	    CURSES_LIB_NAME=ncursesw
	    CURSES_LIB_WIDE=yes
	], [:])
    else
	PKG_CHECK_MODULES([NCURSES], [ncurses], [
	    CURSES_LIB=$NCURSES_LIBS
	    CPPFLAGS="$NCURSES_CFLAGS $CPPFLAGS"
	    CURSES_LIB_NAME=ncurses
	], [:])
    fi
fi

if eval "test x$CURSES_LIB_NAME = x"; then
    AC_CHECK_HEADERS(ncurses.h)

    if test "x$enable_utf8" != xno; then
	OLDLIBS="$LIBS"
	AC_CHECK_TOOL(NCURSESW_CONFIG, ncursesw5-config, no)
	if test "x$NCURSESW_CONFIG" != xno; then
	    CURSES_LIB=`$NCURSESW_CONFIG --libs`
	    LIBS="$CURSES_LIB $LIBS"
	    CPPFLAGS="`$NCURSESW_CONFIG --cflags` $CPPFLAGS"
	    AC_CHECK_LIB(ncursesw, get_wch, [CURSES_LIB_NAME=ncursesw CURSES_LIB_WIDE=yes])
	else
	    AC_CHECK_LIB(ncursesw, get_wch, [CURSES_LIB="-lncursesw" CURSES_LIB_NAME=ncursesw CURSES_LIB_WIDE=yes])
	fi
	LIBS="$OLDLIBS"
    fi

    if eval "test x$CURSES_LIB_NAME = x"; then
	AC_CHECK_LIB(ncurses, initscr, [CURSES_LIB="-lncurses" CURSES_LIB_NAME=ncurses])
    fi
fi

if eval "test x$CURSES_LIB_NAME = x"; then
    AC_CHECK_HEADERS(curses.h)

    if test "x$enable_utf8" != xno; then
	AC_CHECK_LIB(curses, get_wch, [CURSES_LIB="-lcurses" CURSES_LIB_NAME=curses CURSES_LIB_WIDE=yes])
    fi

    if eval "test x$CURSES_LIB_NAME = x"; then
	AC_CHECK_LIB(curses, initscr, [CURSES_LIB="-lcurses" CURSES_LIB_NAME=curses])
    fi
fi

if eval "test x$CURSES_LIB_NAME = x"; then
    AC_MSG_WARN([
*** No curses lib available.  Consider getting the official ncurses
*** distribution from ftp://ftp.gnu.org/pub/gnu/ncurses if you get
*** errors compiling nano.])
else
    AC_MSG_RESULT([Using $CURSES_LIB_NAME as the curses library])
fi

AC_CHECK_LIB([$CURSES_LIB_NAME], [use_default_colors],
	     [AC_DEFINE(HAVE_USE_DEFAULT_COLORS, 1, [Define this if your curses library has the use_default_colors() command.])],
	     [], [$CURSES_LIB])
AC_CHECK_LIB([$CURSES_LIB_NAME], [set_escdelay],
	     [AC_DEFINE(HAVE_SET_ESCDELAY, 1, [Define this if your curses library has the set_escdelay() function.])],
	     [], [$CURSES_LIB])
AC_CHECK_LIB([$CURSES_LIB_NAME], [key_defined],
	     [AC_DEFINE(HAVE_KEY_DEFINED, 1, [Define this if your curses library has the key_defined() command.])],
	     [], [$CURSES_LIB])

dnl Parse any configure options.

LIBS="$LIBS $CURSES_LIB"

AC_SUBST(CURSES_LIB)

if test "x$enable_utf8" != xno && \
    test x$CURSES_LIB_WIDE = xyes && \
    test x$ac_cv_func_iswalnum = xyes && \
    test x$ac_cv_func_iswpunct = xyes && \
    test x$ac_cv_func_mblen = xyes && \
    test x$ac_cv_func_mbstowcs = xyes && \
    test x$ac_cv_func_mbtowc = xyes && \
    test x$ac_cv_func_wctomb = xyes; then
	AC_DEFINE(ENABLE_UTF8, 1, [Define this if your system has sufficient UTF-8 support.])
else
    if test "x$enable_utf8" = xyes; then
	AC_MSG_ERROR([
*** UTF-8 support was requested, but insufficient UTF-8 support was
*** detected in your curses and/or C libraries.  Please verify that your
*** slang was built with UTF-8 support or your curses was built with
*** wide character support, and that your C library was built with wide
*** character support.])
    elif test "x$enable_utf8" != xno; then
	AC_MSG_WARN([
*** Insufficient UTF-8 support was detected in your curses and/or C
*** libraries.  If you want UTF-8 support, please verify that your slang
*** was built with UTF-8 support or your curses was built with wide
*** character support, and that your C library was built with wide
*** character support.])
    fi
fi

AC_CACHE_CHECK([for enhanced regular expression flag], nano_cv_flag_reg_extended,
	       [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <regex.h>]],
						   [[
						     #ifndef REG_ENHANCED
						     error: No REG_ENHANCED support!
						     #endif
						   ]])],
				  [nano_cv_flag_reg_extended="REG_EXTENDED | REG_ENHANCED"],
				  [nano_cv_flag_reg_extended="REG_EXTENDED"])])

AC_DEFINE_UNQUOTED(NANO_REG_EXTENDED, $nano_cv_flag_reg_extended,
		   [Flag(s) to use to get the full range of extended regular expressions])

# Check for word-boundary support (/< and />).
AC_MSG_CHECKING([for GNU-style word boundary regex support])
AC_ARG_WITH(wordbounds,
AS_HELP_STRING([--with-wordbounds], [Use GNU-style word boundary delimiters]),
	[ case "$with_wordbounds" in
	    no)
		AC_MSG_RESULT(no)
		;;
	    *)
		AC_MSG_RESULT(yes)
		# We explicitly don't check if the user forced the option, because
		# this is needed for cross compilers and we can't test the target.
		AC_DEFINE(GNU_WORDBOUNDS, 1, [Define this if the system supports GNU-style word boundaries in regexes.]) gnu_wordbounds=yes,
		;;
	  esac
	], [
	    AC_TRY_RUN([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <regex.h>
int main(void)
{
    regex_t r;
    size_t nmatch;
    regmatch_t pmatch;

    if (regcomp(&r, "\\\\>", $nano_cv_flag_reg_extended|REG_NOSUB))
	return 1;
    if (regexec(&r, "word boundary", nmatch, &pmatch, 0))
	return 1;
    return 0;
}],
	    AC_MSG_RESULT(yes)
	    AC_DEFINE(GNU_WORDBOUNDS, 1, [Define this if the system supports GNU-style word boundaries in regexes.]) gnu_wordbounds=yes,
	    AC_MSG_RESULT(no),
	    AC_MSG_WARN([*** Can't check for GNU-style word boundary support when cross-compiling])
	)
])

if test x$color_support = xyes; then
#    if test x$CURSES_LIB_NAME = xcurses; then
	AC_MSG_CHECKING([whether _XOPEN_SOURCE_EXTENDED is needed])
	AC_TRY_RUN([
#include <curses.h>
int main(void)
{
    int testcolor = COLOR_WHITE;
    return 0;
}], AC_MSG_RESULT(no),
        AC_TRY_RUN([
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif
#include <curses.h>
int main(void)
{
    int testcolor = COLOR_WHITE;
    return 0;
}],
	AC_DEFINE(NEED_XOPEN_SOURCE_EXTENDED, 1, [Define this if you need the _XOPEN_SOURCE_EXTENDED macro for color support])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(not sure)
	AC_MSG_WARN([*** Couldn't successfully compile basic color test with or without _XOPEN_SOURCE_EXTENDED])
	AC_MSG_WARN([*** This build may not compile. Consider configuring with --disable-color or installing ncurses])),
	AC_MSG_WARN([*** Can't check need for _XOPEN_SOURCE_EXTENDED when cross-compiling]))
#    fi
fi

AC_MSG_CHECKING([whether LINES and COLS can be redefined])
AC_TRY_RUN([
#include <curses.h>
int main(void)
{
    LINES = 80;
    COLS = 25;
    return 0;
}],
AC_DEFINE(REDEFINING_MACROS_OK, 1, [Define this if you know your curses library allows LINES and COLS to be redefined to deal with a resizing bug])
AC_MSG_RESULT(yes),
AC_MSG_RESULT(no),
AC_MSG_WARN([*** Can't check for macro redefinability when cross-compiling]))

AS_IF([test "x$enable_libmagic" != "xno"], [
  AC_CHECK_HEADERS([magic.h])
  AC_CHECK_LIB(magic, magic_open)
  AC_CHECK_HEADERS([zlib.h])
  AC_CHECK_LIB(z, inflate)
])

# Check for groff html support.
AC_MSG_CHECKING([for HTML support in groff])
groff -t -mandoc -Thtml </dev/null >/dev/null
if test $? -ne 0 ; then
    AC_MSG_RESULT([no])
    AC_MSG_WARN([*** Will not generate HTML version of man pages  ***
*** Consider installing a newer version of groff with HTML support ***])
    groff_html_support=no
else
    AC_MSG_RESULT([yes])
    groff_html_support=yes
fi
AM_CONDITIONAL(GROFF_HTML, test x$groff_html_support = xyes)

# Check whether this is a git repository.
AC_MSG_CHECKING([whether building from git])
if test -d .git ; then
    AC_MSG_RESULT([yes])
    from_git=yes
else
    AC_MSG_RESULT([no])
    from_git=no
fi
AM_CONDITIONAL(BUILDING_FROM_GIT, test x$from_git = xyes)

AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/sample.nanorc
lib/Makefile
m4/Makefile
po/Makefile.in
src/Makefile
syntax/Makefile
nano.spec
])

AC_OUTPUT
