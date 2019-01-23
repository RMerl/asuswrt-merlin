dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.57)
AC_INIT([lighttpd], [1.4.39], [contact@lighttpd.net])
AC_CONFIG_SRCDIR([src/server.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET

dnl HOW FUCKING BROKEN IS AUTOMAKE...
dnl serial-tests is not recognized before 1.12, and required for serial tests with 1.13, sane version checks "impossible"
dnl if you don't use the default (symlinked) automake on your box, set AUTOMAKE to the path you're using
m4_define([serial_tests], [
    m4_esyscmd([case `${AUTOMAKE:-automake} --version | head -n 1` in
      *1.11.*|*1.10.*|*1.9.*);;
      *) echo serial-tests;;
      esac])
])

AM_INIT_AUTOMAKE([-Wall -Wno-portability -Wno-override foreign dist-xz tar-ustar] serial_tests)

dnl enable with --enable-silent-rules or make V=0 (needs automake >= 1.11)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])


dnl @synopsis TRY_CFLAGS [compiler flags]
dnl @summary check whether compiler supports given flags and adds them to CFLAGS
AC_DEFUN([TRY_CFLAGS],
[dnl
  AC_MSG_CHECKING([if $CC supports $1])
  AC_LANG_PUSH([C])
  ac_try_cflags_saved_cflags="${CFLAGS}"
  CFLAGS="${CFLAGS} $1"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])],
    [
      AC_MSG_ERROR([no])
      CFLAGS="${ac_try_cflags_saved_cflags}"
    ]
  )
  AC_LANG_POP([C])
])

dnl @synopsis TRY_LDFLAGS [linker flags]
dnl @summary check whether linker supports given flags and adds them to LDFLAGS
AC_DEFUN([TRY_LDFLAGS],
[dnl
  AC_MSG_CHECKING([if $LD supports $1])
  AC_LANG_PUSH([C])
  ac_try_ldflags_saved_ldflags="${LDFLAGS}"
  LDFLAGS="${LDFLAGS} $1"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])],
    [
      AC_MSG_ERROR([no])
      LDFLAGS="${ac_try_ldflags_saved_cflags}"
    ]
  )
  AC_LANG_POP([C])
])

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LD
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_CPP
dnl AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl AM_PROG_AR is needed for some linker stuff
dnl AC_USE_SYSTEM_EXTENSIONS requires autoconf 2.60
m4_ifdef([AC_USE_SYSTEM_EXTENSIONS], [AC_USE_SYSTEM_EXTENSIONS])
dnl AM_PROG_AR requires automake 1.11 (and uses AC_COMPILE_IFELSE which wants AC_USE_SYSTEM_EXTENSIONS)
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl check environment
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl AC_CANONICAL_HOST
case $host_os in
	*darwin*|*cygwin*|*aix*|*mingw* ) NO_RDYNAMIC=yes;;
	* ) NO_RDYNAMIC=no;;
esac
AM_CONDITIONAL(NO_RDYNAMIC, test x$NO_RDYNAMIC = xyes)

AC_EXEEXT

dnl libtool
AC_DISABLE_STATIC
AC_ENABLE_SHARED

AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

dnl for solaris
CPPFLAGS="${CPPFLAGS} -D_REENTRANT -D__EXTENSIONS__"

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stdlib.h string.h \
sys/socket.h sys/time.h unistd.h sys/sendfile.h sys/uio.h \
getopt.h sys/epoll.h sys/select.h poll.h sys/poll.h sys/devpoll.h sys/filio.h \
sys/mman.h sys/event.h port.h pwd.h sys/syslimits.h \
sys/resource.h sys/un.h syslog.h sys/prctl.h uuid/uuid.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_CHAR_UNSIGNED
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

AC_CHECK_MEMBER(struct tm.tm_gmtoff,[AC_DEFINE([HAVE_STRUCT_TM_GMTOFF],[1],[gmtoff in struct tm])],,[#include <time.h>])
AC_CHECK_TYPES(struct sockaddr_storage,,,[#include <sys/socket.h>])
AC_CHECK_TYPES(socklen_t,,,[#include <sys/types.h>
#include <sys/socket.h>])

dnl Checks for library functions.
AC_FUNC_FORK
dnl AC_FUNC_MALLOC
dnl AC_FUNC_MMAP
dnl AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([issetugid inet_pton])

if test -z "$PKG_CONFIG"; then
  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
fi

dnl checking for libev
AC_MSG_CHECKING(for libev support)
AC_ARG_WITH(libev,
    AC_HELP_STRING([--with-libev@<:@=PATH@:>@],[Include libev support for fdevent handlers in PATH/include and PATH/lib]),
    [WITH_LIBEV=$withval],[WITH_LIBEV=no])

LIBEV_CFLAGS=""
LIBEV_LIBS=""

if test "$WITH_LIBEV" != "no"; then
	PKG_CHECK_MODULES(LIBEV, libev, [], [
		dnl no pkg-config for libev, searching manually:

		if test "$WITH_LIBEV" != "yes"; then
			LIBEV_CFLAGS="-I$WITH_LIBEV/include"
			LIBEV_LIBS="-L$WITH_LIBEV/lib -lev"
		else
			AC_CHECK_HEADERS([ev.h],[
				AC_CHECK_LIB([ev], [ev_time], [
						LIBEV_CFLAGS=""
						LIBEV_LIBS="-lev"
					],[
					AC_MSG_ERROR([libev not found])
					]
				)],[
					AC_MSG_ERROR([libev not found])
				]
			)
		fi
	])

	AC_DEFINE([HAVE_LIBEV], [1], [libev support])
fi

AC_SUBST(LIBEV_CFLAGS)
AC_SUBST(LIBEV_LIBS)

dnl checking for libsmbclient
AC_MSG_CHECKING(for libsmbclient support)
AC_ARG_WITH(libsmbclient,
    AC_HELP_STRING([--with-libsmbclient@<:@=PATH@:>@],[Include libsmbclient support for fdevent handlers in PATH/include and PATH/bin]),
    [WITH_LIBSMBCLIENT=$withval],[WITH_LIBSMBCLIENT=no])

LIBSMBCLIENT_CFLAGS=""
LIBSMBCLIENT_LIBS=""

if test "$WITH_LIBSMBCLIENT" != "no"; then
	PKG_CHECK_MODULES(LIBSMBCLIENT, libsmbclient, [], [
		dnl no pkg-config for libsmbclient, searching manually:

		if test "$WITH_LIBSMBCLINET" != "yes"; then
			LIBSMBCLIENT_CFLAGS="-I$WITH_LIBSMBCLIENT/include"
			LIBSMBCLIENT_LIBS="-L$WITH_LIBSMBCLIENT/bin -lsmbclient"
		else
			AC_CHECK_HEADERS([libsmbclient.h],[
				AC_CHECK_LIB([smbclient], [smbc_open], [
						LIBSMBCLIENT_CFLAGS=""
						LIBSMBCLIENT_LIBS="-lsmbclient"
					],[
					AC_MSG_ERROR([libsmbclient not found])
					]
				)],[
					AC_MSG_ERROR([libsmbclient not found])
				]
			)
		fi
	])

	AC_DEFINE([HAVE_LIBSMBCLIENT], [1], [libsmbclient support])
    	AC_DEFINE([HAVE_LIBSMBCLIENT_H], [1])
fi

AC_SUBST(LIBSMBCLIENT_CFLAGS)
AC_SUBST(LIBSMBCLIENT_LIBS)


dnl Checks for database.
MYSQL_INCLUDE=""
MYSQL_LIBS=""

AC_MSG_CHECKING(for MySQL support)
AC_ARG_WITH(mysql,
    AC_HELP_STRING([--with-mysql@<:@=PATH@:>@],[Include MySQL support. PATH is the path to 'mysql_config']),
    [WITH_MYSQL=$withval],[WITH_MYSQL=no])

if test "$WITH_MYSQL" != "no"; then
  AC_MSG_RESULT(yes)
  if test "$WITH_MYSQL" = "yes"; then
    AC_PATH_PROG(MYSQL_CONFIG, mysql_config)
  else
    MYSQL_CONFIG=$WITH_MYSQL
  fi

  if test "$MYSQL_CONFIG" = ""; then
    AC_MSG_ERROR(mysql_config is not found)
  fi
  if test \! -x $MYSQL_CONFIG; then
    AC_MSG_ERROR(mysql_config not exists or not executable, use --with-mysql=path-to-mysql_config)
  fi

  if $MYSQL_CONFIG | grep -- '--include' > /dev/null ; then
    MYSQL_INCLUDE="`$MYSQL_CONFIG --include | sed s/\'//g`"
  else
    MYSQL_INCLUDE="`$MYSQL_CONFIG --cflags | sed s/\'//g`"
  fi
  MYSQL_LIBS="`$MYSQL_CONFIG --libs | sed s/\'//g`"

  AC_MSG_CHECKING(for MySQL includes at)
  AC_MSG_RESULT($MYSQL_INCLUDE)

  AC_MSG_CHECKING(for MySQL libraries at)
  AC_MSG_RESULT($MYSQL_LIBS)
dnl check for errmsg.h, which isn't installed by some versions of 3.21
  old_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $MYSQL_INCLUDE"
  AC_CHECK_HEADERS(errmsg.h mysql.h)
  CPPFLAGS="$old_CPPFLAGS"

  AC_DEFINE([HAVE_MYSQL], [1], [mysql support])
else
  AC_MSG_RESULT(no)
fi

AC_SUBST(MYSQL_LIBS)
AC_SUBST(MYSQL_INCLUDE)

dnl Check for LDAP
AC_MSG_CHECKING(for LDAP support)
AC_ARG_WITH(ldap, AC_HELP_STRING([--with-ldap],[enable LDAP support]),
[WITH_LDAP=$withval], [WITH_LDAP=no])
AC_MSG_RESULT([$withval])
if test "$WITH_LDAP" != "no"; then
 AC_CHECK_LIB(ldap, ldap_bind, [
  AC_CHECK_HEADERS([ldap.h],[
    LDAP_LIB=-lldap
    AC_DEFINE([HAVE_LIBLDAP], [1], [libldap])
    AC_DEFINE([HAVE_LDAP_H], [1])
    AC_DEFINE([LDAP_DEPRECATED], [1], [Using deprecated ldap api])
  ])
 ])
 AC_SUBST(LDAP_LIB)
 AC_CHECK_LIB(lber, ber_printf, [
  AC_CHECK_HEADERS([lber.h],[
    LBER_LIB=-llber
    AC_DEFINE([HAVE_LIBLBER], [1], [liblber])
    AC_DEFINE([HAVE_LBER_H], [1])
  ])
 ])
 AC_SUBST(LBER_LIB)
fi

dnl Check for xattr
AC_MSG_CHECKING(for extended attributes support)
AC_ARG_WITH(attr, AC_HELP_STRING([--with-attr],[enable extended attribute support]),
[WITH_ATTR=$withval],[WITH_ATTR=no])
AC_MSG_RESULT($withval)
if test "$WITH_ATTR" != "no"; then
  # libattr (linux only?)
 AC_CHECK_LIB(attr, attr_get, [
	AC_CHECK_HEADERS([attr/attributes.h],[
		ATTR_LIB=-lattr
		AC_DEFINE([HAVE_XATTR], [1], [libattr])
		AC_DEFINE([HAVE_ATTR_ATTRIBUTES_H], [1])
	])
 ])
 AC_SUBST(ATTR_LIB)

  # (Free)BSD extattr
  AC_CHECK_FUNC([extattr_get_file], [
    AC_CHECK_HEADERS([sys/extattr.h],[
      AC_DEFINE([HAVE_EXTATTR], [1], [BSD extended attributes])
      AC_DEFINE([HAVE_SYS_EXTATTR_H], [1])
    ])
  ])
fi

dnl openssl on solaris needs -lsocket -lnsl
AC_SEARCH_LIBS(socket,socket)
AC_SEARCH_LIBS(gethostbyname,nsl socket)
AC_SEARCH_LIBS(hstrerror,resolv)

save_LIBS=$LIBS
AC_SEARCH_LIBS(dlopen,dl,[
  AC_CHECK_HEADERS([dlfcn.h],[
    if test "$ac_cv_search_dlopen" != no; then
      test "$ac_cv_search_dlopen" = "none required" || DL_LIB="$ac_cv_search_dlopen"
    fi

    AC_DEFINE([HAVE_LIBDL], [1], [libdl])
    AC_DEFINE([HAVE_DLFCN_H], [1])
  ])
])
LIBS=$save_LIBS
AC_SUBST(DL_LIB)

dnl Check for valgrind
AC_MSG_CHECKING(for valgrind)
AC_ARG_WITH(valgrind, AC_HELP_STRING([--with-valgrind],[enable internal support for valgrind]),
[WITH_VALGRIND=$withval],[WITH_VALGRIND=no])
AC_MSG_RESULT([$WITH_VALGRIND])
if test "$WITH_VALGRIND" != "no"; then
 AC_CHECK_HEADERS([valgrind/valgrind.h])
fi

dnl Checking for libunwind
AC_MSG_CHECKING(for libunwind)
AC_ARG_WITH(libunwind,
    AC_HELP_STRING([--with-libunwind],[Include libunwind support for backtraces on assert failures]),
    [WITH_LIBUNWIND=$withval],[WITH_LIBUNWIND=no])

if test "$WITH_LIBUNWIND" != "no"; then
  PKG_CHECK_MODULES(LIBUNWIND, libunwind)
  AC_DEFINE(HAVE_LIBUNWIND, 1, [Have libunwind support])
fi

dnl Check for openssl
AC_MSG_CHECKING(for OpenSSL)
AC_ARG_WITH(openssl,
    AC_HELP_STRING([--with-openssl@<:@=DIR@:>@],[Include openssl support (default no)]),
    [WITH_OPENSSL=$withval],[WITH_OPENSSL=no])

if test "$WITH_OPENSSL" != "no"; then
    use_openssl=yes
    if test "$WITH_OPENSSL" != "yes"; then
	CPPFLAGS="$CPPFLAGS -I$WITH_OPENSSL/include"
	LDFLAGS="$LDFLAGS -L$WITH_OPENSSL/lib"
    fi
else
    use_openssl=no
fi
AC_MSG_RESULT([$use_openssl])

AC_ARG_WITH(openssl-includes,
    AC_HELP_STRING([--with-openssl-includes=DIR],[OpenSSL includes]),
    [ use_openssl=yes CPPFLAGS="$CPPFLAGS -I$withval" ]
)

AC_ARG_WITH(openssl-libs,
    AC_HELP_STRING([--with-openssl-libs=DIR],[OpenSSL libraries]),
    [ use_openssl=yes LDFLAGS="$LDFLAGS -L$withval" ]
)

AC_ARG_WITH(kerberos5,
    AC_HELP_STRING([--with-kerberos5],[use Kerberos5 support with OpenSSL]),
    [ use_kerberos=yes ], [use_kerberos=no]
)

if test "x$use_openssl" = "xyes"; then
    if test "x$use_kerberos" != "xyes"; then
        CPPFLAGS="$CPPFLAGS -DOPENSSL_NO_KRB5"
    fi

    AC_CHECK_HEADERS([openssl/ssl.h])
    OLDLIBS="$LIBS"
    AC_CHECK_LIB(crypto, BIO_f_base64, [
      AC_CHECK_LIB(ssl, SSL_new, [ SSL_LIB="-lssl -lcrypto"
				 AC_DEFINE(HAVE_LIBSSL, [], [Have libssl]) ], [], [ -lcrypto "$DL_LIB" ])
    ], [], [])
    LIBS="$OLDLIBS"
    AC_SUBST(SSL_LIB)
fi

AC_MSG_CHECKING(for perl regular expressions support)
AC_ARG_WITH(pcre, AC_HELP_STRING([--with-pcre],[Enable pcre support (default yes)]),
    [WITH_PCRE=$withval],[WITH_PCRE=yes])
AC_MSG_RESULT([$WITH_PCRE])

if test "$WITH_PCRE" != "no"; then
  AC_PATH_PROG(PCRECONFIG, pcre-config)

  if test x"$PCRECONFIG" != x; then
    PCRE_LIB=`$PCRECONFIG --libs`
    CPPFLAGS="$CPPFLAGS `$PCRECONFIG --cflags`"
    AC_DEFINE([HAVE_LIBPCRE], [1], [libpcre])
    AC_DEFINE([HAVE_PCRE_H], [1], [pcre.h])
  else
     AC_MSG_ERROR([pcre-config not found, install the pcre-devel package or build with --without-pcre])
  fi
fi

AC_SUBST(PCRE_LIB)

AC_MSG_CHECKING(for zlib support)
AC_ARG_WITH(zlib, AC_HELP_STRING([--with-zlib],[Enable zlib support for mod_compress]),
    [WITH_ZLIB=$withval],[WITH_ZLIB=yes])
AC_MSG_RESULT([$WITH_ZLIB])

if test "$WITH_ZLIB" != "no"; then
  AC_CHECK_LIB(z, deflate, [
    AC_CHECK_HEADERS([zlib.h],[
      Z_LIB=-lz
      AC_DEFINE([HAVE_LIBZ], [1], [libz])
      AC_DEFINE([HAVE_ZLIB_H], [1])
    ])
  ])
  if test x$Z_LIB = x; then
     AC_MSG_ERROR([zlib-headers and/or libs where not found, install them or build with --without-zlib])
  fi
fi
AC_SUBST(Z_LIB)

AC_MSG_CHECKING(for bzip2 support)
AC_ARG_WITH(bzip2, AC_HELP_STRING([--with-bzip2],[Enable bzip2 support for mod_compress]),
    [WITH_BZIP2=$withval],[WITH_BZIP2=yes])
AC_MSG_RESULT([$WITH_BZIP2])

if test "$WITH_BZIP2" != "no"; then
  AC_CHECK_LIB(bz2, BZ2_bzCompress, [
    AC_CHECK_HEADERS([bzlib.h],[
      BZ_LIB=-lbz2
      AC_DEFINE([HAVE_LIBBZ2], [1], [libbz2])
      AC_DEFINE([HAVE_BZLIB_H], [1])
    ])
  ])
  if test x$BZ_LIB = x; then
     AC_MSG_ERROR([bzip2-headers and/or libs where not found, install them or build with --without-bzip2])
  fi
fi
AC_SUBST(BZ_LIB)

dnl Check for gamin
AC_MSG_CHECKING(for FAM)
AC_ARG_WITH(fam, AC_HELP_STRING([--with-fam],[fam/gamin for reducing number of stat() calls]),
[WITH_FAM=$withval],[WITH_FAM=no])
AC_MSG_RESULT([$WITH_FAM])

if test "$WITH_FAM" != "no"; then
  AC_CHECK_LIB(fam, FAMOpen2, [
    AC_CHECK_HEADERS([fam.h],[
      FAM_LIBS=-lfam
      AC_DEFINE([HAVE_LIBFAM], [1], [libfam])
      AC_DEFINE([HAVE_FAM_H], [1], [fam.h])
    ])
  ])
  if test "x$FAM_LIBS" = x; then
    PKG_CHECK_MODULES(FAM, gamin >= 0.1.0, [
      AC_DEFINE([HAVE_LIBFAM], [1], [libfam])
      AC_DEFINE([HAVE_FAM_H], [1], [fam.h])
    ])
  fi
  OLD_LIBS=$LIBS
  LIBS=$FAM_LIBS
  AC_CHECK_FUNCS([FAMNoExists])
  LIBS=$OLD_LIBS
  
  if test x$FAM_LIBS = x; then
     AC_MSG_ERROR([fam/gamin-headers and/or libs where not found, install them or build with --without-fam])
  fi
fi

AC_MSG_CHECKING(for properties in mod_webdav)
AC_ARG_WITH(webdav-props, AC_HELP_STRING([--with-webdav-props],[properties in mod_webdav]),
[WITH_WEBDAV_PROPS=$withval],[WITH_WEBDAV_PROPS=no])
AC_MSG_RESULT([$WITH_WEBDAV_PROPS])

if test "$WITH_WEBDAV_PROPS" != "no"; then
 PKG_CHECK_MODULES(XML, libxml-2.0, [
    AC_DEFINE([HAVE_LIBXML2], [1], [libxml2])
    AC_DEFINE([HAVE_LIBXML_H], [1], [libxml.h])
 ])
 PKG_CHECK_MODULES(SQLITE, sqlite3, [
    AC_DEFINE([HAVE_SQLITE3], [1], [libsqlite3])
    AC_DEFINE([HAVE_SQLITE3_H], [1], [sqlite3.h])
 ])

 AC_MSG_CHECKING(for locks in mod_webdav)
 AC_ARG_WITH(webdav-locks, AC_HELP_STRING([--with-webdav-locks],[locks in mod_webdav]),
 [WITH_WEBDAV_LOCKS=$withval],[WITH_WEBDAV_LOCKS=no])
 AC_MSG_RESULT([$WITH_WEBDAV_LOCKS])

 if test "$WITH_WEBDAV_LOCKS" != "no"; then
   AC_CHECK_LIB(uuid, uuid_unparse, [
         AC_CHECK_HEADERS([uuid/uuid.h],[
                 UUID_LIBS=-luuid
                 AC_DEFINE([HAVE_UUID], [1], [libuuid])
		 AC_DEFINE([HAVE_UUID_H], [1], [uuid/uuid.h is available])
         ])
 ])
 fi
fi
AC_SUBST(UUID_LIBS)

AC_MSG_CHECKING(for properties in mod_smbdav)
AC_ARG_WITH(smbdav-props, AC_HELP_STRING([--with-smbdav-props],[properties in mod_smbdav]),
[WITH_SMBDAV_PROPS=$withval],[WITH_SMBDAV_PROPS=no])
AC_MSG_RESULT([$WITH_SMBDAV_PROPS])

if test "$WITH_SMBDAV_PROPS" != "no"; then
 PKG_CHECK_MODULES(XML, libxml-2.0, [
    AC_DEFINE([HAVE_LIBXML2], [1], [libxml2])
    AC_DEFINE([HAVE_LIBXML_H], [1], [libxml.h])
 ])
 PKG_CHECK_MODULES(SQLITE, sqlite3, [
    AC_DEFINE([HAVE_SQLITE3], [1], [libsqlite3])
    AC_DEFINE([HAVE_SQLITE3_H], [1], [sqlite3.h])
 ])

 AC_MSG_CHECKING(for locks in mod_smbdav)
 AC_ARG_WITH(smbdav-locks, AC_HELP_STRING([--with-smbdav-locks],[locks in mod_smbdav]),
 [WITH_SMBDAV_LOCKS=$withval],[WITH_SMBDAV_LOCKS=no])
 AC_MSG_RESULT([$WITH_SMBDAV_LOCKS])

 if test "$WITH_SMBDAV_LOCKS" != "no"; then
   AC_CHECK_LIB(uuid, uuid_unparse, [
         AC_CHECK_HEADERS([uuid/uuid.h],[
                 UUID_LIBS=-luuid
                 AC_DEFINE([HAVE_UUID], [1], [libuuid])
		 AC_DEFINE([HAVE_UUID_H], [1], [uuid/uuid.h is available])
         ])
 ])

 fi
fi

dnl Check for gdbm
AC_MSG_CHECKING(for gdbm)
AC_ARG_WITH(gdbm, AC_HELP_STRING([--with-gdbm],[gdbm storage for mod_trigger_b4_dl]),
[WITH_GDBM=$withval],[WITH_GDBM=no])
AC_MSG_RESULT([$WITH_GDBM])

if test "$WITH_GDBM" != "no"; then
 AC_CHECK_LIB(gdbm, gdbm_open, [
         AC_CHECK_HEADERS([gdbm.h],[
                 GDBM_LIB=-lgdbm
                 AC_DEFINE([HAVE_GDBM], [1], [libgdbm])
		 AC_DEFINE([HAVE_GDBM_H], [1])
         ])
 ])
 AC_SUBST(GDBM_LIB)
fi

dnl Check for memcache
AC_MSG_CHECKING(for memcache)
AC_ARG_WITH(memcache, AC_HELP_STRING([--with-memcache],[memcached storage for mod_trigger_b4_dl]),
[WITH_MEMCACHE=$withval],[WITH_MEMCACHE=no])
AC_MSG_RESULT([$WITH_MEMCACHE])
if test "$WITH_MEMCACHE" != "no"; then
 AC_CHECK_LIB(memcache, mc_new, [
         AC_CHECK_HEADERS([memcache.h],[
                 MEMCACHE_LIB=-lmemcache
                 AC_DEFINE([HAVE_MEMCACHE], [1], [libmemcache])
		 AC_DEFINE([HAVE_MEMCACHE_H], [1], [memcache.h])
         ])
 ])
 AC_SUBST(MEMCACHE_LIB)
fi

dnl Check for lua
AC_MSG_CHECKING(if lua-support is requested)
AC_ARG_WITH(lua, AC_HELP_STRING([--with-lua],[lua engine for mod_cml]),
[WITH_LUA=$withval],[WITH_LUA=no])

AC_MSG_RESULT($WITH_LUA)
if test "$WITH_LUA" != "no"; then
 if test "$WITH_LUA" = "yes"; then
  WITH_LUA=lua
 fi
 dnl first check for lua5.1 (debian and others), then lua-5.1 (freebsd), then $WITH_LUA (defaults to lua)
 dnl check "lua" last now that lua5.2 is out
 PKG_CHECK_MODULES(LUA, lua5.1 >= 5.1, [
   AC_DEFINE([HAVE_LUA], [1], [liblua])
   AC_DEFINE([HAVE_LUA_H], [1], [lua.h])
 ],[
  PKG_CHECK_MODULES(LUA, lua-5.1 >= 5.1, [
   AC_DEFINE([HAVE_LUA], [1], [liblua])
   AC_DEFINE([HAVE_LUA_H], [1], [lua.h])
 ],[
   PKG_CHECK_MODULES(LUA, $WITH_LUA >= 5.1, [
   AC_DEFINE([HAVE_LUA], [1], [liblua])
   AC_DEFINE([HAVE_LUA_H], [1], [lua.h])
 ])
 ])
 ])

 AC_SUBST(LUA_CFLAGS)
 AC_SUBST(LUA_LIBS)
fi

dnl search for crypt_r and (fallback) for crypt
save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS([crypt_r],[crypt],[
  AC_DEFINE([HAVE_CRYPT_R], [1], [crypt_r])
  AC_CHECK_HEADERS([crypt.h],[
    AC_DEFINE([HAVE_CRYPT_H], [1], [crypt.h])
  ])

  CRYPT_LIB=$LIBS
],[
  AC_SEARCH_LIBS([crypt],[crypt],[
    AC_DEFINE([HAVE_CRYPT], [1], [crypt])
    AC_CHECK_HEADERS([crypt.h],[
      AC_DEFINE([HAVE_CRYPT_H], [1], [crypt.h])
    ])

    CRYPT_LIB=$LIBS
  ])
])
LIBS=$save_LIBS
AC_SUBST([CRYPT_LIB])

save_LIBS=$LIBS
AC_SEARCH_LIBS(sendfilev,sendfile,[
  if test "$ac_cv_search_sendfilev" != no; then
    test "$ac_cv_search_sendfilev" = "none required" || SENDFILE_LIB="$ac_cv_search_sendfilev"
    AC_DEFINE([HAVE_SENDFILEV], [1], [solaris sendfilev])
  fi
])
LIBS=$save_LIBS
AC_SUBST(SENDFILE_LIB)

case $host_os in
	*mingw* ) LIBS="$LIBS -lwsock32";;
        * ) ;;
esac

AC_CHECK_FUNCS([dup2 getcwd inet_ntoa inet_ntop memset mmap munmap strchr \
		  strdup strerror strstr strtol sendfile  getopt socket lstat \
		  gethostbyname poll epoll_ctl getrlimit chroot \
		  getuid select signal pathconf madvise posix_fadvise posix_madvise \
		  writev sigaction sendfile64 send_file kqueue port_create localtime_r gmtime_r \
		  memset_s explicit_bzero])

AC_MSG_CHECKING(if weak symbols are supported)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
		__attribute__((weak)) void __dummy(void *x) { }
		void f(void *x) { __dummy(x); }
	]])],
	[
		AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_WEAK_SYMBOLS], [1], [weak symbols are supported])
	],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for Large File System support)
AC_ARG_ENABLE(lfs,
 AC_HELP_STRING([--enable-lfs],[Turn on Large File System (default)]),
 [case "${enableval}" in
   yes) CPPFLAGS="${CPPFLAGS} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES" ;;
    no) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-lfs) ;;
  esac],[CPPFLAGS="${CPPFLAGS} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES"
	 enable_lfs=yes])
AC_MSG_RESULT($enableval)

AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(off_t)

if test "x$ac_cv_func_sendfile" = xyes; then
	dnl check if sendfile works
        AC_MSG_CHECKING(if sendfile works)
	if test "x$cross_compiling" = xno; then
	AC_TRY_RUN([
		    #ifdef HAVE_SYS_SENDFILE_H
		    #include <sys/sendfile.h>
		    #endif /* HAVE_SYS_SENDFILE_H */
		    #include <errno.h>
		    int main() {
		      int o = 0;
		      if (-1 == sendfile(-1, 0, &o, 0) && errno == ENOSYS) return -1;
		      return 0;
		    } ],
		    AC_MSG_RESULT(yes),
		    [ AC_MSG_RESULT(no)
		    AC_DEFINE([HAVE_SENDFILE_BROKEN], [1], [broken sendfile]) ] )
	else
	      AC_MSG_RESULT(no, cross-compiling)
	      AC_DEFINE([HAVE_SENDFILE_BROKEN], [1], [broken sendfile])
	fi
fi

dnl Check for IPv6 support

AC_ARG_ENABLE(ipv6,
 AC_HELP_STRING([--disable-ipv6],[disable IPv6 support]),
 [case "${enableval}" in
   yes) ipv6=true ;;
    no) ipv6=false ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-ipv6) ;;
  esac],[ipv6=true])

if test x$ipv6 = xtrue; then
  AC_CACHE_CHECK([for IPv6 support], ac_cv_ipv6_support,
  [AC_TRY_LINK([ #include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>], [struct sockaddr_in6 s; struct in6_addr t=in6addr_any; int i=AF_INET6; s; t.s6_addr[0] = 0; ],
  [ac_cv_ipv6_support=yes], [ac_cv_ipv6_support=no])])

  if test "$ac_cv_ipv6_support" = yes; then
    AC_DEFINE(HAVE_IPV6,1,[Whether to enable IPv6 support])
  fi
fi

# disable mmap by default; if a mmapped file gets truncated, the process gets a SIGBUS signal
# on reading the truncated area which we can't handle (yet).
# lighttpd may always use mmap with files it owns (created tmp files)
AC_ARG_ENABLE(mmap,
  AC_HELP_STRING([--enable-mmap],[use mmap if available (DANGEROUS, allows local users to trigger SIGBUS crashes)]),
  [case "${enableval}" in
   yes) mmap=true ;;
    no) mmap=false ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-mmap) ;;
  esac],[mmap=false])

if test x$mmap = xtrue; then
  AC_DEFINE(ENABLE_MMAP, [1], [Use mmap if available])
fi


AM_CONDITIONAL(CROSS_COMPILING, test "x$cross_compiling" = xyes)

dnl check for fastcgi lib, for the tests only
fastcgi_found=no
AC_CHECK_LIB(fcgi, FCGI_Accept, [
 AC_CHECK_HEADERS([fastcgi.h fastcgi/fastcgi.h],[
   fastcgi_found=yes
 ])
])

AM_CONDITIONAL(CHECK_WITH_FASTCGI, test "x$fastcgi_found" = xyes)


dnl check for extra compiler options (warning options)
if test "${GCC}" = "yes"; then
  TRY_CFLAGS([-Wall -W -Wshadow -pedantic])
  TRY_CFLAGS([-std=gnu99])
fi

AC_ARG_ENABLE(extra-warnings,
 AC_HELP_STRING([--enable-extra-warnings],[enable extra warnings (gcc specific)]),
 [case "${enableval}" in
   yes) extrawarnings=true ;;
    no) extrawarnings=false ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-extra-warnings) ;;
  esac],[extrawarnings=false])

if test x$extrawarnings = xtrue; then
  TRY_CFLAGS([-g -O2 -g2 -Wall -Wmissing-declarations -Wdeclaration-after-statement -Wcast-align -Wsign-compare -Wnested-externs -Wpointer-arith -D_FORTIFY_SOURCE=2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security])
  TRY_LDFLAGS([-Wl,--as-needed])
fi

dnl build version-id
LIGHTTPD_VERSION_ID=`echo $PACKAGE_VERSION | $AWK -F '.' '{print "(" $1 " << 16 | " $2 " << 8 | " $3 ")"}'`
AC_DEFINE_UNQUOTED([LIGHTTPD_VERSION_ID], [$LIGHTTPD_VERSION_ID], [lighttpd-version-id])

AC_CONFIG_FILES([Makefile src/Makefile
		 distribute.sh])
AC_OUTPUT


do_build="mod_cgi mod_fastcgi mod_extforward mod_proxy mod_evhost mod_simple_vhost mod_access mod_alias mod_setenv mod_usertrack mod_auth mod_status mod_accesslog"
do_build="$do_build mod_rrdtool mod_secdownload mod_expire mod_compress mod_dirlisting mod_indexfile mod_userdir mod_webdav mod_staticfile mod_scgi mod_flv_streaming"

plugins="mod_rewrite mod_redirect mod_ssi mod_trigger_b4_dl"
features="regex-conditionals"
if test ! "x$PCRE_LIB" = x; then
	do_build="$do_build $plugins"
	enable_feature="$features"
else
	no_build="$no_build $plugins"
	disable_feature="$features"
fi

plugins="mod_mysql_vhost"
if test ! "x$MYSQL_LIBS" = x; then
	do_build="$do_build $plugins"
else
	no_build="$no_build $plugins"
fi

plugins="mod_cml mod_magnet"
if test ! "x$LUA_LIBS" = x; then
	do_build="$do_build $plugins"
else
	no_build="$no_build $plugins"
fi

features="storage-gdbm"
if test ! "x$GDBM_LIB" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="storage-memcache"
if test ! "x$MEMCACHE_LIB" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="compress-gzip compress-deflate"
if test ! "x$Z_LIB" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="compress-bzip2"
if test ! "x$BZ_LIB" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="auth-ldap"
if test ! "x$LDAP_LIB" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="network-openssl"
if test ! "x$SSL_LIB" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

dnl no crypt call
features="auth-crypt"
if test "$ac_cv_search_crypt" = no; then
	disable_feature="$disable_feature $features"
else
	enable_feature="$enable_feature $features"
fi

features="network-ipv6"
if test "$ac_cv_ipv6_support" = yes; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="large-files"
if test "$enable_lfs" = yes; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="stat-cache-fam"
if test ! "x$FAM_LIBS" = x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi


features="webdav-properties"
if test "x$XML_LIBS" \!= x -a "x$SQLITE_LIBS" \!= x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="webdav-locks"
if test "x$UUID_LIBS" \!= x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="smbdav-properties"
if test "x$XML_LIBS" \!= x -a "x$SQLITE_LIBS" \!= x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

features="smbdav-locks"
if test "x$UUID_LIBS" \!= x; then
	enable_feature="$enable_feature $features"
else
	disable_feature="$disable_feature $features"
fi

dnl output

$ECHO
$ECHO "Plugins:"
$ECHO

$ECHO "enabled: "
for p in $do_build; do
	$ECHO "  $p"
done | sort

$ECHO "disabled: "
for p in $no_build; do
	$ECHO "  $p"
done | sort

$ECHO
$ECHO "Features:"
$ECHO

$ECHO "enabled: "
for p in $enable_feature; do
	$ECHO "  $p"
done | sort

$ECHO "disabled: "
for p in $disable_feature; do
	$ECHO "  $p"
done | sort

$ECHO
