AC_PREREQ(2.59)
AC_INIT([EXIF library], [0.6.19], [libexif-devel@lists.sourceforge.net], [libexif])
AC_CONFIG_SRCDIR([libexif/exif-data.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([auto-m4])
AM_INIT_AUTOMAKE([-Wall gnu 1.9 dist-bzip2 dist-zip check-news])
AM_MAINTAINER_MODE

if test ! -d "$srcdir/m4m"; then
AC_MSG_ERROR([
You are missing the m4m/ directory in your top
$PACKAGE_TARNAME source directory.

You are probably using an ill-maintained CVS tree.
Running

    cd $srcdir
    cvs co m4m

and re-running autogen.sh might help.
])
fi

GP_CHECK_SHELL_ENVIRONMENT
GP_CONFIG_MSG([Build])
GP_CONFIG_MSG([Source code location],[${srcdir}])

dnl ---------------------------------------------------------------------------
dnl Advanced information about versioning:
dnl   * "Writing shared libraries" by Mike Hearn
dnl         http://plan99.net/~mike/writing-shared-libraries.html
dnl   * libtool.info chapter "Versioning"
dnl   * libtool.info chapter "Updating library version information"
dnl ---------------------------------------------------------------------------
dnl Versioning:
dnl  - CURRENT (Major):  Increment if the interface has changes. AGE is always
dnl                      *changed* at the same time.
dnl  - AGE (Micro):      Increment if any interfaces have been added; set to 0
dnl		         if any interfaces have been removed. Removal has 
dnl                      precedence over adding, so set to 0 if both happened.
dnl                      It denotes upward compatibility.
dnl  - REVISION (Minor): Increment any time the source changes; set to 
dnl			 0 if you incremented CURRENT.
dnl
dnl  To summarize. Any interface *change* increment CURRENT. If that interface
dnl  change does not break upward compatibility (ie it is an addition), 
dnl  increment AGE, Otherwise AGE is reset to 0. If CURRENT has changed, 
dnl  REVISION is set to 0, otherwise REVISION is incremented.
dnl ---------------------------------------------------------------------------
dnl C:A:R
dnl 12:0:1   0.6.13
dnl 13:1:0   added EXIF_DATA_OPTION_DONT_CHANGE_MAKER_NOTE (for 0.6.14)
dnl 14:2:0   added XP_ WinXP tags (for 0.6.15)
dnl 14:2:1   0.6.17
dnl 15:3:0   added exif_loader_get_buf (for 0.6.18)
dnl 15:3:1   0.6.19
LIBEXIF_CURRENT=15
LIBEXIF_AGE=3
LIBEXIF_REVISION=1
AC_SUBST([LIBEXIF_AGE])
AC_SUBST([LIBEXIF_REVISION])
AC_SUBST([LIBEXIF_CURRENT])
AC_SUBST([LIBEXIF_CURRENT_MIN],[`expr $LIBEXIF_CURRENT - $LIBEXIF_AGE`])
LIBEXIF_VERSION_INFO="$LIBEXIF_CURRENT:$LIBEXIF_REVISION:$LIBEXIF_AGE"
AC_SUBST([LIBEXIF_VERSION_INFO])

AC_PROG_CC
AC_C_CONST
AC_C_INLINE
dnl FIXME: AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL
AM_CPPFLAGS="$CPPFLAGS"
GP_CONFIG_MSG([Compiler],[${CC}])


dnl Create a stdint.h-like file containing size-specific integer definitions
dnl that will always be available
AX_NEED_STDINT_H([libexif/_stdint.h])


dnl ------------------------------------------------------------------------
dnl Whether we're supposed to ship binaries in the tarball
dnl ------------------------------------------------------------------------

ship_binaries=false
AC_ARG_ENABLE([ship-binaries],
[AS_HELP_STRING([--enable-ship-binaries],
[Whether to ship binaries in the tarball [default=no]])],[
	if test x$enableval = xyes; then
		ship_binaries=true
	fi
])
AM_CONDITIONAL([SHIP_BINARIES],[$ship_binaries])
GP_CONFIG_MSG([Ship binaries in tarball],[$ship_binaries])


dnl ---------------------------------------------------------------------------
dnl Whether -lm is required for our math functions
dnl ---------------------------------------------------------------------------

# we need sqrt and pow which may be in libm
# We cannot use AC_CHECK_FUNC because if CFLAGS contains
# -Wall -Werror here the check fails because
# char *sqrt() conflicts with double sqrt(double xx)

# Start by assuming -lm is needed, because it's possible that the little
# test program below will be optimized to in-line floating point code that
# doesn't require -lm, whereas the library itself cannot be so optimized
# (this actually seems to be the case on x86 with gcc 4.2). Assuming the
# reverse means that -lm could be needed but wouldn't be detected below.

LIBS_orig="$LIBS"
LIBS="$LIBS -lm"
AC_MSG_CHECKING([for math functions in libm])
AC_LINK_IFELSE([
	#include <math.h>
	int main() {
	  double s = sqrt(0);
	  double p = pow(s,s);
	  return (int)p;
	}
], [AC_MSG_RESULT(yes)], [
	AC_MSG_RESULT(no)
	LIBS="$LIBS_orig"
	AC_MSG_CHECKING([for math functions without libm])
	AC_LINK_IFELSE([
		#include <math.h>
		int main() {
		  double s = sqrt(0);
		  double p = pow(s,s);
		  return (int)p;
		}
	], [
		AC_MSG_RESULT(yes)
	],[
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([*** Could not find sqrt() & pow() functions])
	])
])

# doc support
GP_CHECK_DOC_DIR
GP_CHECK_DOXYGEN

# Whether to enable the internal docs build.
#
# This takes quite some time due to the generation of lots of call
# graphs, so it is disabled by default.
set_enable_internal_docs=no
AC_ARG_ENABLE([internal-docs], [dnl
AS_HELP_STRING([--enable-internal-docs], 
[Build internal code docs if doxygen available])], [dnl
dnl If either --enable-foo nor --disable-foo were given, execute this.
  if   test "x$enableval" = xno \
    || test "x$enableval" = xoff \
    || test "x$enableval" = xfalse; 
  then
    set_enable_internal_docs=no
  elif test "x$enableval" = xyes \
    || test "x$enableval" = xon \
    || test "x$enableval" = xtrue
  then
    set_enable_internal_docs=yes
  fi
])
AC_MSG_CHECKING([whether to create internal code docs])
AC_MSG_RESULT([${set_enable_internal_docs}])
AM_CONDITIONAL([ENABLE_INTERNAL_DOCS], [test "x${set_enable_internal_docs}" = "xyes"])


# ---------------------------------------------------------------------------
# i18n support
# ---------------------------------------------------------------------------
ALL_LINGUAS="be cs da de en_CA en_GB es fr it ja nl pl pt pt_BR ru sk sq sr sv vi zh_CN"
AM_PO_SUBDIRS
GP_GETTEXT_HACK([${PACKAGE}-${LIBEXIF_CURRENT_MIN}],
                [Lutz Mueller and others])
AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
AM_ICONV()
GP_GETTEXT_FLAGS()


dnl ---------------------------------------------------------------------------
dnl Thread-safe functions
dnl ---------------------------------------------------------------------------
AC_CHECK_FUNCS(localtime_r)

dnl ---------------------------------------------------------------------------
dnl Compiler/Linker Options and Warnings
dnl ---------------------------------------------------------------------------
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)"
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_builddir)"
AM_LDFLAGS="$LDFLAGS"
if test "x$GCC" = "xyes"; then
    AM_CFLAGS="$AM_CFLAGS -ansi -pedantic-error"
    AM_CXXFLAGS="$AM_CXXFLAGS -ansi -pedantic-error"
    AM_CPPFLAGS="$AM_CPPFLAGS -g -Wall -Wmissing-declarations -Wmissing-prototypes"
    AM_LDFLAGS="$AM_LDFLAGS -g -Wall"
fi



AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_LDFLAGS)


dnl ---------------------------------------------------------------------------
dnl Output files
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([  po/Makefile.in
  Makefile
  libexif.spec
  libexif/Makefile
  test/Makefile
  test/nls/Makefile
  m4m/Makefile
  doc/Makefile
  doc/Doxyfile
  doc/Doxyfile-internals
  libexif.pc
  libexif-uninstalled.pc
  binary/Makefile
  contrib/Makefile
  contrib/examples/Makefile
])
AC_OUTPUT

GP_CONFIG_OUTPUT
