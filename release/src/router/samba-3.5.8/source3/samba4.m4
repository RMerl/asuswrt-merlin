AC_SUBST(BLDSHARED)
smbtorture4_path="bin/smbtorture4"
smbtorture4_option="-t bin/smbtorture4"
m4_include(build/m4/public.m4)

m4_include(../m4/check_python.m4)

AC_SAMBA_PYTHON_DEVEL([
SMB_EXT_LIB(EXT_LIB_PYTHON, [$PYTHON_LDFLAGS], [$PYTHON_CFLAGS])
SMB_ENABLE(EXT_LIB_PYTHON,YES)
SMB_ENABLE(LIBPYTHON,YES)
],[
AC_MSG_ERROR([Python not found. Please install Python 2.x and its development headers/libraries.])
])

AC_MSG_CHECKING(python library directory)
pythondir=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_python_lib(1, 0, '\\${prefix}')"`
AC_MSG_RESULT($pythondir)

AC_SUBST(pythondir)

SMB_EXT_LIB(LIBREPLACE_EXT, [${LIBDL} ${CRYPT_LIBS}])
SMB_ENABLE(LIBREPLACE_EXT)

SMB_EXT_LIB(LIBREPLACE_NETWORK, [${LIBREPLACE_NETWORK_LIBS}])
SMB_ENABLE(LIBREPLACE_NETWORK)

SMB_SUBSYSTEM(LIBREPLACE,
	[${LIBREPLACE_OBJS}],
	[LIBREPLACE_EXT LIBREPLACE_NETWORK],
	[-I../lib/replace])

LIBREPLACE_HOSTCC_OBJS=`echo ${LIBREPLACE_OBJS} |sed -e 's/\.o/\.ho/g'`

SMB_SUBSYSTEM(LIBREPLACE_HOSTCC,
	[${LIBREPLACE_HOSTCC_OBJS}],
	[],
	[-I../lib/replace])

m4_include(lib/smbreadline/readline.m4)
m4_include(heimdal_build/internal.m4)
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
m4_include(../nsswitch/nsstest.m4)
m4_include(../pidl/config.m4)
AC_ZLIB([
SMB_EXT_LIB(ZLIB, [${ZLIB_LIBS}])
],[
SMB_INCLUDE_MK(lib/zlib.mk)
])


AC_CONFIG_FILES(../source4/lib/registry/registry.pc)
AC_CONFIG_FILES(../source4/librpc/dcerpc.pc)
AC_CONFIG_FILES(../librpc/ndr.pc)
AC_CONFIG_FILES(../lib/torture/torture.pc)
AC_CONFIG_FILES(../source4/auth/gensec/gensec.pc)
AC_CONFIG_FILES(../source4/param/samba-hostconfig.pc)
AC_CONFIG_FILES(../source4/librpc/dcerpc_samr.pc)
AC_CONFIG_FILES(../source4/librpc/dcerpc_atsvc.pc)

m4_include(../source4/min_versions.m4)

SMB_EXT_LIB_FROM_PKGCONFIG(LIBTALLOC, talloc >= TALLOC_MIN_VERSION,
	[],
	[
		SMB_INCLUDE_MK(../lib/talloc/config.mk)
	]
)
# Tallocdir isn't always set by the Samba3 c
tallocdir=../lib/talloc
AC_SUBST(tallocdir)
CFLAGS="$CFLAGS -I../lib/talloc"

SMB_EXT_LIB_FROM_PKGCONFIG(LIBTDB, tdb >= TDB_MIN_VERSION,
	[],
	[
		m4_include(../lib/tdb/libtdb.m4)
		SMB_INCLUDE_MK(../lib/tdb/config.mk)
	]
)

SMB_INCLUDE_MK(../lib/tdb/python.mk) 

SMB_EXT_LIB_FROM_PKGCONFIG(LIBTEVENT, tevent = TEVENT_REQUIRED_VERSION,
	[],[m4_include(../lib/tevent/samba.m4)]
)

SMB_EXT_LIB_FROM_PKGCONFIG(LIBLDB, ldb = LDB_REQUIRED_VERSION,
	[
		SMB_INCLUDE_MK(lib/ldb/ldb_ildap/config.mk)
		SMB_INCLUDE_MK(lib/ldb/tools/config.mk)
		define_ldb_modulesdir=no
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
		ldbdir="\$(abspath \$(srcdir)/../source4/lib/ldb)"
		AC_SUBST(ldbdir)
		m4_include(lib/ldb/sqlite3.m4)
		m4_include(lib/ldb/libldb.m4)
		SMB_INCLUDE_MK(lib/ldb/config.mk)
		AC_CONFIG_FILES(../source4/lib/ldb/ldb.pc)
	]
)
SMB_INCLUDE_MK(lib/ldb/python.mk) 

# Not sure why we need this..
SMB_ENABLE(swig_ldb,YES)

# Don't build wbinfo twice
SMB_ENABLE(wbinfo, NO)

m4_include(lib/tls/config.m4)
m4_include(torture/libnetapi/config.m4)

dnl m4_include(auth/kerberos/config.m4)
m4_include(auth/gensec/config.m4)
m4_include(smbd/process_model.m4)
m4_include(ntvfs/posix/config.m4)
m4_include(ntvfs/unixuid/config.m4)
m4_include(auth/config.m4)
m4_include(kdc/config.m4)
m4_include(ntvfs/sysdep/config.m4)
m4_include(../nsswitch/config.m4)

AC_SUBST(INTERN_LDFLAGS)
AC_SUBST(INSTALL_LINK_FLAGS)
if test $USESHARED = "true";
then
	INTERN_LDFLAGS="-L\$(shliboutputdir) -L\${builddir}/bin/static"
	INSTALL_LINK_FLAGS="-Wl,-rpath-link,\$(shliboutputdir)";
else
	INTERN_LDFLAGS="-L\${builddir}/bin/static -L\$(shliboutputdir)"
fi

dnl Samba 4 files
AC_SUBST(LD)
AC_LIBREPLACE_SHLD_FLAGS
dnl Remove -L/usr/lib/? from LDFLAGS and LIBS
LIB_REMOVE_USR_LIB(LDFLAGS)
LIB_REMOVE_USR_LIB(LIBS)
LIB_REMOVE_USR_LIB(KRB5_LIBS)

dnl Remove -I/usr/include/? from CFLAGS and CPPFLAGS
CFLAGS_REMOVE_USR_INCLUDE(CFLAGS)
CFLAGS_REMOVE_USR_INCLUDE(CPPFLAGS)
SMB_WRITE_MAKEVARS(samba4-config.mk, [prefix exec_prefix CPPFLAGS LDSHFLAGS POPT_OBJ CFLAGS TALLOC_OBJ POPT_LIBS srcdir builddir])
		 
oldbuilddir="$builddir"
builddir="$builddir/../source4"
oldsrcdir="$srcdir"
srcdir="$srcdir/../source4"
AC_SUBST(srcdir)
AC_SUBST(builddir)
SMB_WRITE_PERLVARS(../source4/build/smb_build/config.pm)
builddir="$oldbuilddir"
srcdir="$oldsrcdir"

echo "configure: creating ../source4/config.mk"
cat >../source4/config.mk<<CEOF
# config.mk - Autogenerated by configure, DO NOT EDIT!
$SMB_INFO_EXT_LIBS
$SMB_INFO_SUBSYSTEMS
$SMB_INFO_LIBRARIES
CEOF

AC_OUTPUT_COMMANDS([
cd ${srcdir}/../source4
$PERL -I${builddir} -I${builddir}/build \
    -I. -Ibuild \
    build/smb_build/main.pl --output=../source3/samba4-data.mk main.mk || exit $?
cd ../source3
],[
srcdir="$srcdir"
builddir="$builddir"
PERL="$PERL"

export PERL
export srcdir
export builddir
])

AC_CONFIG_COMMANDS([Makefile-samba4], [
echo "include samba4.mk" >> ${builddir}/Makefile
])

