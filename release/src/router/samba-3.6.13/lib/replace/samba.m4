AC_LIBREPLACE_BROKEN_CHECKS
AC_LIBREPLACE_NETWORK_CHECKS

SMB_EXT_LIB(LIBREPLACE_EXT, [${LIBDL} ${CRYPT_LIBS}])
SMB_ENABLE(LIBREPLACE_EXT)

SMB_EXT_LIB(LIBREPLACE_NETWORK, [${LIBREPLACE_NETWORK_LIBS}])
SMB_ENABLE(LIBREPLACE_NETWORK)

# remove leading ./
LIBREPLACE_DIR=`echo ${libreplacedir} |sed -e 's/^\.\///g'`

# remove leading srcdir .. we are looking for the relative
# path within the samba source tree or wherever libreplace is.
# We need to make sure the object is not forced to end up in
# the source directory because we might be using a separate
# build directory.
LIBREPLACE_DIR=`echo ${LIBREPLACE_DIR} | sed -e "s|^$srcdir/||g"`

LIBREPLACE_OBJS="${LIBREPLACEOBJ}"

SMB_SUBSYSTEM(LIBREPLACE,
	[${LIBREPLACE_OBJS}],
	[LIBREPLACE_EXT LIBREPLACE_NETWORK],
	[-Ilib/replace])

LIBREPLACE_HOSTCC_OBJS=`echo ${LIBREPLACE_OBJS} |sed -e 's/\.o/\.ho/g'`

SMB_SUBSYSTEM(LIBREPLACE_HOSTCC,
	[${LIBREPLACE_HOSTCC_OBJS}],
	[],
	[-Ilib/replace])

AC_CHECK_FUNCS([syslog],,[AC_MSG_ERROR([Required function not found])])
