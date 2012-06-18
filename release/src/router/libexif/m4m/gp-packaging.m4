AC_DEFUN([GPKG_CHECK_RPM],
[
AC_ARG_WITH([rpmbuild],
[AS_HELP_STRING([--with-rpmbuild=PATH],
[Program to use for building RPMs])])

if test -x "${with_rpmbuild}"
then
    RPMBUILD="${with_rpmbuild}"
    AC_MSG_CHECKING([for rpmbuild or rpm])
    AC_MSG_RESULT([${RPMBUILD} (from parameter)])
else
    AC_MSG_RESULT([using autodetection])
    AC_CHECK_PROGS(RPMBUILD, [rpmbuild rpm], false)
    AC_MSG_CHECKING([for rpmbuild or rpm])
    AC_MSG_RESULT([${RPMBUILD} (autodetected)])
fi
AC_SUBST([RPMBUILD])
AM_CONDITIONAL([ENABLE_RPM], [test "$RPMBUILD" != "false"])

# whether libusb-devel is installed or not defines whether the RPM
# packages we're going to build will depend on libusb and libusb-devel
# RPM packages or not.
AM_CONDITIONAL([RPM_LIBUSB_DEVEL], [rpm -q libusb-devel > /dev/null 2>&1])
])

AC_DEFUN([GPKG_CHECK_LINUX],
[
	# effective_target has to be determined in advance
	AC_REQUIRE([AC_NEED_BYTEORDER_H])

	is_linux=false
	case "$effective_target" in 
		*linux*)
			is_linux=true
			;;
	esac
	AM_CONDITIONAL([HAVE_LINUX], ["$is_linux"])

	# required for docdir
	# FIXME: Implicit dependency
	# AC_REQUIRE(GP_CHECK_DOC_DIR)

	AC_ARG_WITH([hotplug-doc-dir],
	[AS_HELP_STRING([--with-hotplug-doc-dir=PATH],
	[Where to install hotplug scripts as docs [default=autodetect]])])

	AC_MSG_CHECKING([for hotplug doc dir])
	if test "x${with_hotplug_doc_dir}" != "x"
	then # given as parameter
	    hotplugdocdir="${with_hotplug_doc_dir}"
	    AC_MSG_RESULT([${hotplugdocdir} (from parameter)])
	else # start at docdir
	    hotplugdocdir="${docdir}/linux-hotplug"
	    AC_MSG_RESULT([${hotplugdocdir} (default)])
	fi
	AC_SUBST([hotplugdocdir])

	AC_ARG_WITH([hotplug-usermap-dir],
	[AS_HELP_STRING([--with-hotplug-usermap-dir=PATH],
	[Where to install hotplug scripts as docs [default=autodetect]])])

	AC_MSG_CHECKING([for hotplug usermap dir])
	if test "x${with_hotplug_usermap_dir}" != "x"
	then # given as parameter
	    hotplugusermapdir="${with_hotplug_usermap_dir}"
	    AC_MSG_RESULT([${hotplugusermapdir} (from parameter)])
	else # start at docdir
	    hotplugusermapdir="${docdir}/linux-hotplug"
	    AC_MSG_RESULT([${hotplugusermapdir} (default)])
	fi

	AC_SUBST([hotplugusermapdir])
])
