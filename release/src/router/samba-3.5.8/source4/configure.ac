dnl -*- mode: m4-mode -*-
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.54)

AC_INIT([samba],[4],[samba-technical@samba.org])

AC_CONFIG_SRCDIR([include/includes.h])
AC_CONFIG_HEADER(include/config_tmp.h)
AC_DEFINE(CONFIG_H_IS_FROM_SAMBA,1,[Marker for samba's config.h.])

# Configuration rules.
m4_include(build/m4/env.m4)
m4_include(../lib/replace/samba.m4)
m4_include(lib/smbreadline/readline.m4)
m4_include(heimdal_build/config.m4)
m4_include(../lib/util/fault.m4)
m4_include(../lib/util/signal.m4)
m4_include(../lib/util/util.m4)
m4_include(../lib/util/fsusage.m4)
m4_include(../lib/util/xattr.m4)
m4_include(../lib/util/capability.m4)
m4_include(../lib/util/time.m4)
m4_include(../lib/popt/samba.m4)
m4_include(../lib/util/charset/config.m4)
m4_include(lib/socket/config.m4)
m4_include(../lib/zlib/zlib.m4)
AC_ZLIB([
SMB_EXT_LIB(ZLIB, [${ZLIB_LIBS}])
],[
SMB_INCLUDE_MK(lib/zlib.mk)
])
m4_include(../nsswitch/nsstest.m4)
m4_include(../pidl/config.m4)
m4_include(client/config.m4)

AC_CONFIG_FILES(lib/registry/registry.pc)
AC_CONFIG_FILES(librpc/dcerpc.pc)
AC_CONFIG_FILES(../librpc/ndr.pc)
AC_CONFIG_FILES(../librpc/ndr_standard.pc)
AC_CONFIG_FILES(../lib/torture/torture.pc)
AC_CONFIG_FILES(auth/gensec/gensec.pc)
AC_CONFIG_FILES(param/samba-hostconfig.pc)
AC_CONFIG_FILES(librpc/dcerpc_samr.pc)
AC_CONFIG_FILES(librpc/dcerpc_atsvc.pc)

m4_include(min_versions.m4)

SMB_INCLUDED_LIB_PKGCONFIG(LIBTALLOC, talloc >= TALLOC_MIN_VERSION, [],
	[
		m4_include(../lib/talloc/libtalloc.m4)
		SMB_INCLUDE_MK(../lib/talloc/config.mk)
	]
)

SMB_INCLUDED_LIB_PKGCONFIG(LIBTDB, tdb >= TDB_MIN_VERSION,
	[
		AC_MSG_CHECKING([for Python TDB bindings])
		if $PYTHON -c "from tdb import open" 2>/dev/null; then
			AC_MSG_RESULT(yes)
		else
			SMB_INCLUDE_MK(../lib/tdb/python.mk) 
			AC_MSG_RESULT(no)
		fi
	 ],
	[
		m4_include(../lib/tdb/libtdb.m4)
		SMB_INCLUDE_MK(../lib/tdb/config.mk)
		SMB_INCLUDE_MK(../lib/tdb/python.mk) 
	]
)

SMB_INCLUDED_LIB_PKGCONFIG(LIBTEVENT, tevent = TEVENT_REQUIRED_VERSION,
	[],[m4_include(../lib/tevent/samba.m4)]
)

SMB_INCLUDED_LIB_PKGCONFIG(LIBLDB, ldb = LDB_REQUIRED_VERSION,
	[
		SMB_INCLUDE_MK(lib/ldb/ldb_ildap/config.mk)
		SMB_INCLUDE_MK(lib/ldb/tools/config.mk)
		define_ldb_modulesdir=no
		AC_MSG_CHECKING([for Python LDB bindings])
		if $PYTHON -c "import ldb" 2>/dev/null; then
			AC_MSG_RESULT(yes)
		else
			SMB_INCLUDE_MK(lib/ldb/python.mk) 
			AC_MSG_RESULT(no)
		fi

	],
	[
		# Here we need to do some tricks
		# with AC_CONFIG_COMMANDS_PRE
		# as that's the deferrs the commands
		# to location after $prefix and $exec_prefix
		# have usefull values and directly before
		# creating config.status.
		#
		# The 'eval eval echo' trick is used to
		# actually get the raw absolute directory
		# path as this is needed in config.h
		define_ldb_modulesdir=yes
		AC_CONFIG_COMMANDS_PRE([
		if test x"$define_ldb_modulesdir" = x"yes";then
			LDB_MODULESDIR=`eval eval echo ${modulesdir}/ldb`
			AC_DEFINE_UNQUOTED(LDB_MODULESDIR, "${LDB_MODULESDIR}" , [ldb Modules directory])
		fi
		])
		ldbdir=lib/ldb
		AC_SUBST(ldbdir)
		m4_include(lib/ldb/sqlite3.m4)
		m4_include(lib/ldb/libldb.m4)
		SMB_INCLUDE_MK(lib/ldb/config.mk)
		AC_CONFIG_FILES(lib/ldb/ldb.pc)
		SMB_INCLUDE_MK(lib/ldb/python.mk) 
	])

m4_include(lib/tls/config.m4)
m4_include(torture/libnetapi/config.m4)

dnl m4_include(auth/kerberos/config.m4)
m4_include(auth/gensec/config.m4)
m4_include(smbd/process_model.m4)
m4_include(ntvfs/posix/config.m4)
m4_include(ntvfs/unixuid/config.m4)
m4_include(../lib/socket_wrapper/config.m4)
m4_include(../lib/nss_wrapper/config.m4)
m4_include(../lib/uid_wrapper/config.m4)
m4_include(auth/config.m4)
m4_include(kdc/config.m4)
m4_include(ntvfs/sysdep/config.m4)
m4_include(../nsswitch/config.m4)

#################################################
# add *_CFLAGS only for the real build
CFLAGS="${CFLAGS} ${DEVELOPER_CFLAGS}"

#################################################
# final configure stuff

AC_MSG_CHECKING([configure summary])
AC_TRY_RUN([#include "${srcdir-.}/../tests/summary.c"],
           AC_MSG_RESULT(yes),
	   AC_MSG_ERROR([summary failure. Aborting config]); exit 1;,
	   AC_MSG_WARN([cannot run when cross-compiling]))

LIBS=`echo $LIBS | sed -e 's/ *//g'`
if test x"$LIBS" != x""; then
	echo "LIBS: $LIBS"
	AC_MSG_WARN([the global \$LIBS variable contains some libraries!])
	AC_MSG_WARN([this should not happen, please report to samba-technical@lists.samba.org!])
	AC_MSG_ERROR([only _EXT macros from aclocal.m4 should be used!])
fi

dnl Remove -L/usr/lib/? from LDFLAGS and LIBS
LIB_REMOVE_USR_LIB(LDFLAGS)
LIB_REMOVE_USR_LIB(LIBS)

dnl Remove -I/usr/include/? from CFLAGS and CPPFLAGS
CFLAGS_REMOVE_USR_INCLUDE(CFLAGS)
CFLAGS_REMOVE_USR_INCLUDE(CPPFLAGS)

AC_SUBST(ac_default_prefix)

for d in build/smb_build bin include ; do
    test -d ${builddir}/$d || AS_MKDIR_P(${builddir}/$d)
done

AC_SUBST(INTERN_LDFLAGS)
AC_SUBST(INSTALL_LINK_FLAGS)
if test $USESHARED = "true";
then
	INTERN_LDFLAGS="-L\$(shliboutputdir) -L\${builddir}/bin/static"
	INSTALL_LINK_FLAGS="-Wl,-rpath-link,\$(shliboutputdir)";
else
	INTERN_LDFLAGS="-L\${builddir}/bin/static -L\$(shliboutputdir)"
fi

builddir_headers=""
	
if test "x$ac_abs_srcdir" != "x$ac_abs_builddir"
then
	builddir_headers="-I\$(builddir)/include -I\$(builddir) -I\$(builddir)/lib ";
fi

CPPFLAGS="$builddir_headers-I\$(srcdir)/include -I\$(srcdir) -I\$(srcdir)/lib -I\$(srcdir)/../lib/replace -I\$(srcdir)/../lib/talloc -I\$(srcdir)/.. -D_SAMBA_BUILD_=4 -DHAVE_CONFIG_H $CPPFLAGS"

SMB_WRITE_PERLVARS(build/smb_build/config.pm)

echo "configure: creating config.mk"
cat >config.mk<<CEOF
# config.mk - Autogenerated by configure, DO NOT EDIT!
$SMB_INFO_EXT_LIBS
$SMB_INFO_SUBSYSTEMS
$SMB_INFO_LIBRARIES
CEOF

SMB_BUILD_RUN(data.mk)
AC_OUTPUT

cmp include/config_tmp.h include/config.h >/dev/null 2>&1
CMP_RET=$?
if test $CMP_RET != 0; then
	cp include/config_tmp.h include/config.h
fi

SMB_WRITE_MAKEVARS(mkconfig.mk)

if test $USESHARED = true
then
	echo "To run binaries without installing, set the following environment variable:"
	echo "$LIB_PATH_VAR=$builddir/bin/shared"
fi

echo
echo "To build Samba, run $MAKE"
