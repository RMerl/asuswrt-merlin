dnl AM_CONFIG_HEADER(config.h)

AC_PREREQ([2.65])
AC_INIT([GeoIP], [1.6.2],[support@maxmind.com],[GeoIP])
AC_GNU_SOURCE
AM_INIT_AUTOMAKE(1.11.0)
AC_CONFIG_SRCDIR([libGeoIP/GeoIP.c])
GEOIP_VERSION_INFO=`echo $VERSION | awk -F. '{ printf "%d:%d:%d", $1+$2, $3, $2 }'`
AC_SUBST(GEOIP_VERSION_INFO)

AC_PROG_CC
AC_PROG_LIBTOOL

AC_CHECK_TYPE(byte,[AC_DEFINE(HAVE_BYTE_TYPEDEF)],[])
AC_CHECK_TYPE(ushort,[AC_DEFINE(HAVE_USHORT_TYPEDEF)],[])
AC_CHECK_TYPE(ulong,[AC_DEFINE(HAVE_ULONG_TYPEDEF)],[])
AC_CHECK_TYPE(u16,[AC_DEFINE(HAVE_U16_TYPEDEF)],[])
AC_CHECK_TYPE(u32,[AC_DEFINE(HAVE_U32_TYPEDEF)],[])

AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

AC_C_BIGENDIAN([AC_DEFINE(BIG_ENDIAN_HOST,1)],[AC_DEFINE(LITTLE_ENDIAN_HOST,1)])

AC_CHECK_FUNC(getopt, AC_DEFINE(HAVE_GETOPT))
AC_CHECK_FUNC(gettimeofday, AC_DEFINE(HAVE_GETTIMEOFDAY))
AC_CHECK_FUNC(pread, AC_DEFINE(HAVE_PREAD))
AC_CHECK_FUNC(vasprintf, AC_DEFINE(HAVE_VASPRINTF))
AC_CHECK_FUNC(vsnprintf, AC_DEFINE(HAVE_VSNPRINTF))
AC_CHECK_FUNC(vsprintf, AC_DEFINE(HAVE_VSPRINTF))

AC_CHECK_FUNC(gethostbyname, AC_DEFINE(HAVE_GETHOSTBYNAME),
	AC_CHECK_LIB(nsl, gethostbyname, AC_DEFINE(HAVE_GETHOSTBYNAME)
	LIBS="${LIBS} -lsocket -lnsl"))

AC_CHECK_FUNC(gethostbyname_r, [
	AC_DEFINE(HAVE_GETHOSTBYNAME_R)
	# We look for the one that returns `int'.
	# Hopefully this check is robust enough.
	AC_EGREP_HEADER(int.*gethostbyname_r, netdb.h, [
		AC_DEFINE(GETHOSTBYNAME_R_RETURNS_INT)])
	])

AC_CHECK_FUNCS([ memset memchr munmap strrchr socket strchr strdup strstr ])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_MMAP

AC_ARG_ENABLE([data-files],
    AS_HELP_STRING([--disable-data-files], [Install library without installing data files]))

AS_IF([test "x$enable_data_files" != xno],
    AC_SUBST([GEOIP_DB_FILE],[GeoIP.dat]))

AC_OUTPUT([
Makefile
GeoIP.spec
geoip.pc
libGeoIP/Makefile
apps/Makefile
man/Makefile
test/Makefile
])
