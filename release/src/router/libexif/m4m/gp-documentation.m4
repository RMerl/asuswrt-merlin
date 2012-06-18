dnl
dnl check where to install documentation
dnl
dnl determines documentation "root directory", i.e. the directory
dnl where all documentation will be placed in
dnl

AC_DEFUN([GP_DOC_GENERAL],[dnl
AC_MSG_CHECKING([whether to build any docs])
AC_ARG_ENABLE([docs], [dnl
AS_HELP_STRING([--disable-docs], [whether to create any documentation])], [dnl
case "$enableval" in
	yes|true|on) gp_build_docs="yes" ;;
	*) gp_build_docs="no" ;;
esac
],[dnl
gp_build_docs="yes"
])dnl
AC_MSG_RESULT([${gp_build_docs}])
AM_CONDITIONAL([BUILD_DOCS], [test "x${gp_build_docs}" = "xyes"])
])dnl

AC_DEFUN([GP_CHECK_DOC_DIR],
[
AC_REQUIRE([GP_DOC_GENERAL])dnl
AC_BEFORE([$0], [GP_BUILD_GTK_DOCS])dnl
AC_BEFORE([$0], [GP_CHECK_DOXYGEN])dnl

AC_ARG_WITH([doc-dir],
[AS_HELP_STRING([--with-doc-dir=PATH],
[Where to install docs  [default=autodetect]])])

# check for the main ("root") documentation directory
AC_MSG_CHECKING([main docdir])

if test "x${with_doc_dir}" != "x"
then # docdir is given as parameter
    docdir="${with_doc_dir}"
    AC_MSG_RESULT([${docdir} (from parameter)])
else # otherwise invent a docdir hopefully compatible with system policy
    if test -d "/usr/share/doc"
    then
        maindocdir='${prefix}/share/doc'
        AC_MSG_RESULT([${maindocdir} (FHS style)])
    elif test -d "/usr/doc"
    then
        maindocdir='${prefix}/doc'
        AC_MSG_RESULT([${maindocdir} (old style)])
    else
        maindocdir='${datadir}/doc'
        AC_MSG_RESULT([${maindocdir} (default value)])
    fi
    AC_MSG_CHECKING([package docdir])
    # check whether to include package version into documentation path
    # FIXME: doesn't work properly.
    if ls -d /usr/{share/,}doc/make-[0-9]* > /dev/null 2>&1
    then
        docdir="${maindocdir}/${PACKAGE}-${VERSION}"
        AC_MSG_RESULT([${docdir} (redhat style)])
    else
        docdir="${maindocdir}/${PACKAGE}"
        AC_MSG_RESULT([${docdir} (default style)])
    fi
fi

AC_SUBST([docdir])
])dnl

dnl
dnl check whether to build docs and where to:
dnl
dnl * determine presence of prerequisites (only gtk-doc for now)
dnl * determine destination directory for HTML files
dnl

AC_DEFUN([GP_BUILD_GTK_DOCS],
[
# docdir has to be determined in advance
AC_REQUIRE([GP_CHECK_DOC_DIR])

# ---------------------------------------------------------------------------
# gtk-doc: We use gtk-doc for building our documentation. However, we
#          require the user to explicitely request the build.
# ---------------------------------------------------------------------------
try_gtkdoc=false
gtkdoc_msg="no (not requested)"
have_gtkdoc=false
AC_ARG_ENABLE([docs],
[AS_HELP_STRING([--enable-docs],
[Use gtk-doc to build documentation [default=no]])],[
	if test x$enableval = xyes; then
		try_gtkdoc=true
	fi
])
if $try_gtkdoc; then
	AC_PATH_PROG([GTKDOC],[gtkdoc-mkdb])
	if test -n "${GTKDOC}"; then
		have_gtkdoc=true
		gtkdoc_msg="yes"
	else
		gtkdoc_msg="no (http://www.gtk.org/rdp/download.html)"
	fi
fi
AM_CONDITIONAL([ENABLE_GTK_DOC], [$have_gtkdoc])
GP_CONFIG_MSG([build API docs with gtk-doc],[$gtkdoc_msg])


# ---------------------------------------------------------------------------
# Give the user the possibility to install html documentation in a 
# user-defined location.
# ---------------------------------------------------------------------------
AC_ARG_WITH([html-dir],
[AS_HELP_STRING([--with-html-dir=PATH],
[Where to install html docs [default=autodetect]])])

AC_MSG_CHECKING([for html dir])
if test "x${with_html_dir}" = "x" ; then
    htmldir="${docdir}/html"
    AC_MSG_RESULT([${htmldir} (default)])
else
    htmldir="${with_html_dir}"
    AC_MSG_RESULT([${htmldir} (from parameter)])
fi
AC_SUBST([htmldir])
apidocdir="${htmldir}/api"
AC_SUBST([apidocdir}])

])dnl


dnl doxygen related stuff
dnl look for tools
dnl define substitutions for Doxyfile.in
AC_DEFUN([GP_CHECK_DOXYGEN],[dnl
AC_REQUIRE([GP_CHECK_DOC_DIR])dnl
AC_PATH_PROG([DOT], [dot], [false])
AC_PATH_PROG([DOXYGEN], [doxygen], [false])
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$DOXYGEN" != "xfalse"])
AM_CONDITIONAL([HAVE_DOT], [test "x$DOT" != "xfalse"])
if test "x$DOT" != "xfalse"; then
	AC_SUBST([HAVE_DOT],[YES])
else
	AC_SUBST([HAVE_DOT],[NO])
fi
AC_SUBST([HTML_APIDOC_DIR], ["${PACKAGE_TARNAME}-api.html"])
AC_SUBST([DOXYGEN_OUTPUT_DIR], [doxygen-output])
AC_SUBST([HTML_APIDOC_INTERNALS_DIR], ["${PACKAGE_TARNAME}-internals.html"])
])dnl

