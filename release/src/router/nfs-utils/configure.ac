dnl Process this file with autoconf to produce a configure script.
dnl
AC_INIT([linux nfs-utils],[1.1.6],[linux-nfs@vger.kernel.org],[nfs-utils])
AC_CANONICAL_BUILD([])
AC_CANONICAL_HOST([])
AC_CONFIG_MACRO_DIR(aclocal)
AM_INIT_AUTOMAKE
AC_PREREQ(2.59)
AC_PREFIX_DEFAULT(/usr)
AM_MAINTAINER_MODE

dnl *************************************************************
dnl * Define the set of applicable options
dnl *************************************************************
AC_ARG_WITH(release,
	[AC_HELP_STRING([--with-release=XXX], [set release to XXX [1]])],
	RELEASE=$withval,
	RELEASE=1)
	AC_SUBST(RELEASE)
AC_ARG_WITH(statedir,
	[  --with-statedir=/foo    use state dir /foo [/var/lib/nfs]],
	statedir=$withval,
	statedir=/var/lib/nfs)
	AC_SUBST(statedir)
AC_ARG_WITH(statduser,
	[AC_HELP_STRING([--with-statduser=rpcuser],
                        [statd to run under @<:@rpcuser or nobody@:>@]
	)],
	statduser=$withval,
	if test "x$cross_compiling" = "xno"; then
		if grep -s '^rpcuser:' /etc/passwd > /dev/null; then
			statduser=rpcuser
		else
			statduser=nobody
		fi
	else
		statduser=nobody
	fi)
	AC_SUBST(statduser)
AC_ARG_WITH(start-statd,
	[AC_HELP_STRING([--with-start-statd=scriptname],
			[When an nfs filesystems is mounted with locking, run this script]
	)],
	startstatd=$withval,
	startstatd=/usr/sbin/start-statd
	)
	AC_SUBST(startstatd)
	AC_DEFINE_UNQUOTED(START_STATD, "$startstatd", [Define this to a script which can start statd on mount])
AC_ARG_ENABLE(nfsv3,
	[AC_HELP_STRING([--enable-nfsv3],
                        [enable support for NFSv3 @<:@default=yes@:>@])],
	enable_nfsv3=$enableval,
	enable_nfsv3=yes)
	if test "$enable_nfsv3" = yes; then
		AC_DEFINE(NFS3_SUPPORTED, 1, [Define this if you want NFSv3 support compiled in])
	else
		enable_nfsv3=
	fi
	AC_SUBST(enable_nfsv3)
AC_ARG_ENABLE(nfsv4,
	[AC_HELP_STRING([--enable-nfsv4],
                        [enable support for NFSv4 @<:@default=yes@:>@])],
	enable_nfsv4=$enableval,
	enable_nfsv4=yes)
	if test "$enable_nfsv4" = yes; then
		AC_DEFINE(NFS4_SUPPORTED, 1, [Define this if you want NFSv4 support compiled in])
		IDMAPD=idmapd
	else
		enable_nfsv4=
		IDMAPD=
	fi
	AC_SUBST(IDMAPD)
	AC_SUBST(enable_nfsv4)
	AM_CONDITIONAL(CONFIG_NFSV4, [test "$enable_nfsv4" = "yes"])
AC_ARG_ENABLE(gss,
	[AC_HELP_STRING([--enable-gss],
                        [enable support for rpcsec_gss @<:@default=yes@:>@])],
	enable_gss=$enableval,
	enable_gss=yes)
	if test "$enable_gss" = yes; then
		AC_DEFINE(GSS_SUPPORTED, 1, [Define this if you want rpcsec_gss support compiled in])
		GSSD=gssd
		SVCGSSD=svcgssd
	else
		enable_gss=
		GSSD=
		SVCGSSD=
	fi
	AC_SUBST(GSSD)
	AC_SUBST(SVCGSSD)
	AC_SUBST(enable_gss)
	AM_CONDITIONAL(CONFIG_GSS, [test "$enable_gss" = "yes"])
AC_ARG_ENABLE(kprefix,
	[AC_HELP_STRING([--enable-kprefix], [install progs as rpc.knfsd etc])],
	test "$enableval" = "yes" && kprefix=k,
	kprefix=)
	AC_SUBST(kprefix)
AC_ARG_WITH(rpcgen,
	[AC_HELP_STRING([--with-rpcgen=internal], [use internal rpcgen instead of system one])],
	rpcgen_path=$withval,
	rpcgen_path=yes )
	RPCGEN_PATH=
	if test "$rpcgen_path" == "yes"; then
	    for p in /usr/local/bin/rpcgen /usr/bin/rpcgen /bin/rpcgen
	    do if test -f $p ; then RPCGEN_PATH=$p ; break; fi ; done
	elif test "$rpcgen_path" != "internal"; then
	    RPCGEN_PATH=$rpcgen_path
	fi
	AC_SUBST(RPCGEN_PATH)
	AM_CONDITIONAL(CONFIG_RPCGEN, [test "$RPCGEN_PATH" == ""])
AC_ARG_ENABLE(uuid,
	[AC_HELP_STRING([--disable-uuid], [Exclude uuid support to avoid buggy libblkid])],
	if test "$enableval" = "yes" ; then choose_blkid=yes; else choose_blkid=no; fi,
	choose_blkid=default)
AC_ARG_ENABLE(mount,
	[AC_HELP_STRING([--enable-mount],
			[Create mount.nfs and don't use the util-linux mount(8) functionality. @<:@default=yes@:>@])],
	enable_mount=$enableval,
	enable_mount=yes)
	AM_CONDITIONAL(CONFIG_MOUNT, [test "$enable_mount" = "yes"])
AC_ARG_ENABLE(tirpc,
	[AC_HELP_STRING([--enable-tirpc],
			[enable use of TI-RPC @<:@default=no@:>@])],
	enable_tirpc=$enableval,
	enable_tirpc=no)
AC_ARG_ENABLE(ipv6,
	[AC_HELP_STRING([--enable-ipv6],
                        [enable support for IPv6 @<:@default=no@:>@])],
	enable_ipv6=$enableval,
	enable_ipv6=no)
	if test "$enable_ipv6" = yes; then
		AC_DEFINE(IPV6_SUPPORTED, 1, [Define this if you want IPv6 support compiled in])
	else
		enable_ipv6=
	fi
	AC_SUBST(enable_ipv6)
	AM_CONDITIONAL(CONFIG_IPV6, [test "$enable_ipv6" = "yes"])

dnl Check for TI-RPC library and headers
AC_LIBTIRPC

# Check whether user wants TCP wrappers support
AC_TCP_WRAPPERS

# Arrange for large-file support
AC_SYS_LARGEFILE

AC_CONFIG_SRCDIR([support/include/config.h.in])
AC_CONFIG_HEADERS([support/include/config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

if test "x$cross_compiling" = "xno"; then
	CC_FOR_BUILD=${CC_FOR_BUILD-${CC-gcc}}
else
	CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
fi

AC_SUBST(CC_FOR_BUILD)

AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(LD, ld)

AC_HEADER_STDC([])
AC_GNULIBC
AC_BSD_SIGNALS

dnl *************************************************************
dnl * Check for required libraries
dnl *************************************************************

AC_CHECK_FUNC([gethostbyname], ,
              [AC_CHECK_LIB([nsl], [gethostbyname], [LIBNSL="-lnsl"])])
AC_SUBST(LIBNSL)

AC_CHECK_FUNC([connect], ,
              [AC_CHECK_LIB([socket], [connect], [LIBSOCKET="-lsocket"],
                    [AC_MSG_ERROR([Function 'socket' not found.])], [$LIBNSL])])

AC_CHECK_FUNC([getaddrinfo], ,
              [AC_MSG_ERROR([Function 'getaddrinfo' not found.])])

AC_CHECK_FUNC([getrpcbynumber], ,
              [AC_MSG_ERROR([Function 'getrpcbynumber' not found.])])

AC_CHECK_FUNC([getservbyname], ,
              [AC_MSG_ERROR([Function 'getservbyname' not found.])])

AC_CHECK_LIB([crypt], [crypt], [LIBCRYPT="-lcrypt"])

if test "$enable_nfsv4" = yes; then
  dnl check for libevent libraries and headers
  AC_LIBEVENT

  dnl check for nfsidmap libraries and headers
  AC_LIBNFSIDMAP

  dnl librpcsecgss already has a dependency on libgssapi,
  dnl but we need to make sure we get the right version
  if test "$enable_gss" = yes; then
    AC_RPCSEC_VERSION
  fi
fi

if test "$knfsd_cv_glibc2" = no; then
    AC_CHECK_LIB(bsd, daemon, [LIBBSD="-lbsd"])
fi

if test "$choose_blkid" != no; then 
   AC_CHECK_LIB(blkid, blkid_get_library_version, [LIBBLKID="-lblkid"], AC_MSG_ERROR([libblkid needed]))
   AC_CHECK_HEADER(blkid/blkid.h, , AC_MSG_ERROR([Cannot find libblkid header file blkid/blkid.h]))
   AC_BLKID_VERS
   if test $choose_blkid = yes; then
      use_blkid=1
      test $libblkid_cv_is_recent = no && AC_MSG_WARN([libblkid is old and may cause mountd to leak memory])
   else
      if test $libblkid_cv_is_recent = yes
      then use_blkid=1
      else use_blkid=0
	   AC_MSG_WARN([uuid support disabled as libblkid is too old])
      fi
    fi
    AC_DEFINE_UNQUOTED(USE_BLKID, $use_blkid, [Define if you want to use blkid to find uuid of filesystems])
fi
AC_SUBST(LIBSOCKET)
AC_SUBST(LIBCRYPT)
AC_SUBST(LIBBSD)
AC_SUBST(LIBBLKID)

if test "$enable_gss" = yes; then
  dnl 'gss' requires getnameinfo - at least for gssd_proc.c
  AC_CHECK_FUNC([getnameinfo], , [AC_MSG_ERROR([GSSAPI support requires 'getnameinfo' function])])

  dnl 'gss' also depends on nfsidmap.h - at least for svcgssd_proc.c
  AC_LIBNFSIDMAP

  AC_CHECK_HEADERS([spkm3.h], ,
                   [AC_MSG_WARN([Could not locate SPKM3 header; will not have SPKM3 support])])

  dnl Check for Kerberos V5
  AC_KERBEROS_V5

  dnl Invoked after AC_KERBEROS_V5; AC_LIBRPCSECGSS needs to have KRBLIBS set
  AC_LIBRPCSECGSS

fi

dnl Check for IPv6 support
AC_IPV6

dnl *************************************************************
dnl Check for headers
dnl *************************************************************
AC_CHECK_HEADERS([arpa/inet.h fcntl.h libintl.h limits.h \
                 malloc.h memory.h netdb.h netinet/in.h paths.h \
                 stdlib.h string.h sys/file.h sys/ioctl.h sys/mount.h \
                 sys/param.h sys/socket.h sys/time.h sys/vfs.h \
                 syslog.h unistd.h com_err.h et/com_err.h \
                 ifaddrs.h])

dnl *************************************************************
dnl Checks for typedefs, structures, and compiler characteristics
dnl *************************************************************
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl *************************************************************
dnl Check for functions
dnl *************************************************************
AC_FUNC_ALLOCA
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_FUNC_GETMNTENT
AC_PROG_GCC_TRADITIONAL
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_HEADER_MAJOR
#AC_FUNC_MALLOC
AC_FUNC_MEMCMP
#AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([alarm atexit dup2 fdatasync ftruncate getcwd \
               gethostbyaddr gethostbyname gethostname getmntent \
               getnameinfo getrpcbyname \
               gettimeofday hasmntopt inet_ntoa innetgr memset mkdir pathconf \
               realpath rmdir select socket strcasecmp strchr strdup \
               strerror strrchr strtol strtoul sigprocmask])


dnl *************************************************************
dnl Check for data sizes
dnl *************************************************************
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(socklen_t)


dnl *************************************************************
dnl Export some path names to config.h
dnl *************************************************************
AC_DEFINE_UNQUOTED(NFS_STATEDIR, "$statedir", [This defines the location of the NFS state files. Warning: this must match definitions in config.mk!])

if test "x$cross_compiling" = "xno"; then
	CFLAGS_FOR_BUILD=${CFLAGS_FOR_BUILD-"$CFLAGS"}
	CXXFLAGS_FOR_BUILD=${CXXFLAGS_FOR_BUILD-"$CXXFLAGS"}
	CPPFLAGS_FOR_BUILD=${CPPFLAGS_FOR_BUILD-"$CPPFLAGS"}
	LDFLAGS_FOR_BUILD=${LDFLAGS_FOR_BUILD-"$LDFLAGS"}
else
	CFLAGS_FOR_BUILD=${CFLAGS_FOR_BUILD-""}
	CXXFLAGS_FOR_BUILD=${CXXFLAGS_FOR_BUILD-""}
	CPPFLAGS_FOR_BUILD=${CPPFLAGS_FOR_BUILD-""}
	LDFLAGS_FOR_BUILD=${LDFLAGS_FOR_BUILD-""}
fi

AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

AC_SUBST(CFLAGS_FOR_BUILD)
AC_SUBST(CXXFLAGS_FOR_BUILD)
AC_SUBST(CPPFLAGS_FOR_BUILD)
AC_SUBST(LDFLAGS_FOR_BUILD)

dnl *************************************************************
dnl Set up "global" CFLAGS
dnl *************************************************************
dnl Use architecture-specific compile flags
dnl (We use $host and not $build in case we are cross-compiling)
dnl *************************************************************
dnl Note: we no longer have arch specific compile flags, but 
dnl the stub is left here in case they are needed one day.
case $host in
  *)
    ARCHFLAGS="" ;;
esac

my_am_cflags="-Wall -Wstrict-prototypes $ARCHFLAGS -pipe"

AC_SUBST([AM_CFLAGS], ["$my_am_cflags"])

# Make sure that $ACLOCAL_FLAGS are used during a rebuild
AC_SUBST([ACLOCAL_AMFLAGS], ["-I $ac_macro_dir \$(ACLOCAL_FLAGS)"])

AC_CONFIG_FILES([
	Makefile
	linux-nfs/Makefile
	support/Makefile
	support/export/Makefile
	support/include/nfs/Makefile
	support/include/rpcsvc/Makefile
	support/include/sys/fs/Makefile
	support/include/sys/Makefile
	support/include/Makefile
	support/misc/Makefile
	support/nfs/Makefile
	tools/Makefile
	tools/locktest/Makefile
	tools/nlmtest/Makefile
	tools/rpcdebug/Makefile
	tools/rpcgen/Makefile
	utils/Makefile
	utils/exportfs/Makefile
	utils/gssd/Makefile
	utils/idmapd/Makefile
	utils/mount/Makefile
	utils/mountd/Makefile
	utils/nfsd/Makefile
	utils/nfsstat/Makefile
	utils/showmount/Makefile
	utils/statd/Makefile])
AC_OUTPUT

