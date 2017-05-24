# This file is part of MiniDLNA.
#
# MiniDLNA is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# MiniDLNA is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with MiniDLNA; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.
AC_INIT(MiniDLNA,1.1.3,,minidlna)
#LT_INIT

AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES([yes])

m4_ifdef([AC_USE_SYSTEM_EXTENSIONS], [AC_USE_SYSTEM_EXTENSIONS])

#MiniDLNA

AM_ICONV
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.18)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Default string definitions
AC_DEFINE_UNQUOTED([OS_URL],"http://sourceforge.net/projects/minidlna/",[OS URL])
AC_DEFINE_UNQUOTED([ROOTDEV_MANUFACTURER],"Justin Maggard", [Manufacturer])
AC_DEFINE_UNQUOTED([ROOTDEV_MANUFACTURERURL],"http://www.netgear.com/", [Manufacturer URL])
AC_DEFINE_UNQUOTED([ROOTDEV_MODELNAME],"Windows Media Connect compatible (MiniDLNA)", [Model name])
AC_DEFINE_UNQUOTED([ROOTDEV_MODELDESCRIPTION],"MiniDLNA on " OS_NAME, [Model description])
AC_DEFINE_UNQUOTED([ROOTDEV_MODELURL],OS_URL,[Model URL])


################################################################################################################
# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
m4_ifdef([AC_TYPE_UINT8_T], [AC_TYPE_UINT8_T])
m4_ifdef([AC_TYPE_INT32_T], [AC_TYPE_INT32_T])
m4_ifdef([AC_TYPE_UINT32_T], [AC_TYPE_UINT32_T])
m4_ifdef([AC_TYPE_UINT64_T], [AC_TYPE_UINT64_T])
m4_ifdef([AC_TYPE_SSIZE_T], [AC_TYPE_SSIZE_T])
AC_STRUCT_DIRENT_D_TYPE
AC_STRUCT_ST_BLOCKS
AC_HEADER_STDBOOL
AC_C_BIGENDIAN

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([gethostname getifaddrs gettimeofday inet_ntoa memmove memset mkdir realpath select sendfile setlocale socket strcasecmp strchr strdup strerror strncasecmp strpbrk strrchr strstr strtol strtoul])

#
# Check for struct ip_mreqn
#
AC_MSG_CHECKING(for struct ip_mreqn)
AC_TRY_COMPILE([#include <netinet/in.h>], [
    struct ip_mreqn mreq;
    mreq.imr_address.s_addr = 0;
], [
    # Yes, we have it...
    AC_MSG_RESULT(yes)
    AC_DEFINE([HAVE_STRUCT_IP_MREQN],[],[Support for struct ip_mreqn])
], [
    # We'll just have to try and use struct ip_mreq
    AC_MSG_RESULT(no)
    AC_MSG_CHECKING(for struct ip_mreq)
    AC_TRY_COMPILE([#include <netinet/in.h>], [
        struct ip_mreq mreq;
        mreq.imr_interface.s_addr = 0;
    ], [
        # Yes, we have it...
        AC_MSG_RESULT(yes)
        AC_DEFINE([HAVE_STRUCT_IP_MREQ],[],[Support for struct ip_mreq])
    ], [
        # No multicast support
        AC_MSG_ERROR([No multicast support])
    ])
])

################################################################################################################
# Special include directories
case $host in
    *-*-darwin*)
        DARWIN_OS=1
        SEARCH_DIR="/opt/local"
        INCLUDE_DIR="$SEARCH_DIR/include"
        ;;
    *-*-solaris*)
        AC_DEFINE([SOLARIS], [1], [we are on solaris])
        ;;
    *-*-cygwin*)
        CYGWIN_OS=1
        ;;
    *-*-freebsd*)
        FREEBSD_OS=1
        ;;
    *-*-openbsd*)
        OPENBSD_OS=1
        ;;
    *-*-linux*)
        if test -f /etc/redhat-release; then
            INCLUDE_DIR="/usr/include/ffmpeg"
        fi
        ;;
esac

AC_CHECK_HEADERS(syscall.h sys/syscall.h mach/mach_time.h)
AC_MSG_CHECKING([for __NR_clock_gettime syscall])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [
            #include <asm/unistd.h>
        ],
        [
            #ifndef __NR_clock_gettime
            #error
            #endif
        ]
    )],
    [
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_CLOCK_GETTIME_SYSCALL], [1], [Whether the __NR_clock_gettime syscall is defined])
    ],
    [
        AC_MSG_RESULT([no])
        AC_SEARCH_LIBS([clock_gettime], [rt], [AC_DEFINE([HAVE_CLOCK_GETTIME], [1], [use clock_gettime])],)
    ])

AC_CHECK_HEADER(linux/netlink.h,
    [AC_DEFINE([HAVE_NETLINK],[1],[Support for Linux netlink])], [], [#include <sys/socket.h>])

################################################################################################################
### Library checks

CPPFLAGS_SAVE="$CPPFLAGS"
for dir in "" /usr/local $INCLUDE_DIR; do
    if test -n "$dir"; then
        CPPFLAGS="$CPPFLAGS_SAVE -I$dir"
    fi
    AC_CHECK_HEADERS([libavutil/avutil.h ffmpeg/libavutil/avutil.h libav/libavutil/avutil.h avutil.h ffmpeg/avutil.h libav/avutil.h], [HAVE_LIBAVUTIL=1])
    if test -z "$HAVE_LIBAVUTIL"; then
        unset ac_cv_header_avutil_h
        unset ac_cv_header_libavutil_avutil_h
        unset ac_cv_header_ffmpeg_avutil_h
        unset ac_cv_header_ffmpeg_libavutil_avutil_h
        unset ac_cv_header_libav_avutil_h
        unset ac_cv_header_libav_libavutil_avutil_h
        continue
    fi
    break
done
if test -z "$HAVE_LIBAVUTIL"; then
    AC_MSG_ERROR([libavutil headers not found or not usable])
fi

CPPFLAGS_SAVE="$CPPFLAGS"
for dir in "" /usr/local $INCLUDE_DIR; do
    if test -n "$dir"; then
        CPPFLAGS="$CPPFLAGS_SAVE -I$dir"
    fi
    AC_CHECK_HEADERS([libavcodec/avcodec.h ffmpeg/libavcodec/avcodec.h libav/libavcodec/avcodec.h avcodec.h ffmpeg/avcodec.h libav/avcodec.h], [HAVE_LIBAVCODEC=1])
    if test -z "$HAVE_LIBAVCODEC"; then
        unset ac_cv_header_avcodec_h
        unset ac_cv_header_libavcodec_avcodec_h
        unset ac_cv_header_ffmpeg_avcodec_h
        unset ac_cv_header_ffmpeg_libavcodec_avcodec_h
        unset ac_cv_header_libav_avcodec_h
        unset ac_cv_header_libav_libavcodec_avcodec_h
        continue
    fi
    break
done
if test -z "$HAVE_LIBAVCODEC"; then
    AC_MSG_ERROR([libavcodec headers not found or not usable])
fi

CPPFLAGS_SAVE="$CPPFLAGS"
for dir in "" /usr/local $INCLUDE_DIR; do
    if test -n "$dir"; then
        CPPFLAGS="$CPPFLAGS_SAVE -I$dir"
    fi
    AC_CHECK_HEADERS([libavformat/avformat.h ffmpeg/libavformat/avformat.h libav/libavformat/avformat.h avformat.h ffmpeg/avformat.h libav/avformat.h], [HAVE_LIBAVFORMAT=1])
    if test -z "$HAVE_LIBAVFORMAT"; then
        unset ac_cv_header_avformat_h
        unset ac_cv_header_libavformat_avformat_h
        unset ac_cv_header_ffmpeg_avformat_h
        unset ac_cv_header_ffmpeg_libavformat_avformat_h
        unset ac_cv_header_libav_avformat_h
        unset ac_cv_header_libav_libavformat_avformat_h
        continue
    fi
    break
done
if test -z "$HAVE_LIBAVFORMAT"; then
    AC_MSG_ERROR([libavformat headers not found or not usable])
fi

CPPFLAGS_SAVE="$CPPFLAGS"
for dir in "" /usr/local $INCLUDE_DIR; do
    if test -n "$dir"; then
        CPPFLAGS="$CPPFLAGS -I$dir"
    fi
    AC_CHECK_HEADERS([jpeglib.h sqlite3.h libexif/exif-loader.h id3tag.h ogg/ogg.h vorbis/codec.h FLAC/metadata.h],,[unset $as_ac_Header; break],)
    if test x"$ac_cv_header_jpeglib_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    elif test x"$ac_cv_header_sqlite3_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    elif test x"$ac_cv_header_libexif_exif_loader_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    elif test x"$ac_cv_header_id3tag_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    elif test x"$ac_cv_header_ogg_ogg_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    elif test x"$ac_cv_header_vorbis_codec_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    elif test x"$ac_cv_header_FLAC_metadata_h" != x"yes"; then
        CPPFLAGS="$CPPFLAGS_SAVE"
        continue
    else
        break;
    fi
done
test x"$ac_cv_header_jpeglib_h" != x"yes" && AC_MSG_ERROR([libjpeg headers not found or not usable])
test x"$ac_cv_header_sqlite3_h" != x"yes" && AC_MSG_ERROR([libsqlite3 headers not found or not usable])
test x"$ac_cv_header_libexif_exif_loader_h" != x"yes" && AC_MSG_ERROR([libexif headers not found or not usable])
test x"$ac_cv_header_id3tag_h" != x"yes" && AC_MSG_ERROR([libid3tag headers not found or not usable])
test x"$ac_cv_header_ogg_ogg_h" != x"yes" && AC_MSG_ERROR([libogg headers not found or not usable])
test x"$ac_cv_header_vorbis_codec_h" != x"yes" && AC_MSG_ERROR([libvorbis headers not found or not usable])
test x"$ac_cv_header_FLAC_metadata_h" != x"yes" && AC_MSG_ERROR([libFLAC headers not found or not usable])

CFLAGS_SAVE="$CFLAGS"
CFLAGS="$CFLAGS -Wall -Werror"
AC_MSG_CHECKING([if we should use the daemon() libc function])
AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
         [
             #include <stdlib.h>
             #include <unistd.h>
         ],
         [
            return daemon(0, 0);
         ]
     )],
     [
         AC_DEFINE([USE_DAEMON], [1], 
                   [use the system's builtin daemon()])
         AC_MSG_RESULT([yes])
     ],
     [
         AC_MSG_RESULT([no])
     ])

AC_MSG_CHECKING([if scandir declaration requires const char cast])
AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
         [
             #include <stdlib.h>
             #include <sys/types.h>
             #include <dirent.h>
         ],
         [
            int filter(struct dirent *d);
            struct dirent **ptr = NULL;
            char *name = NULL;
            (void)scandir(name, &ptr, filter, alphasort);
         ]
     )],
     [
         AC_MSG_RESULT([no])
     ],
     [
         AC_DEFINE([SCANDIR_CONST], [1], 
                   [scandir needs const char cast])

         AC_MSG_RESULT([yes])
     ])

AC_MSG_CHECKING([for linux sendfile support])
AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
         [
             #include <sys/types.h>
             #include <sys/sendfile.h>
         ],
         [
             int tofd = 0, fromfd = 0;
             off_t offset;
             size_t total = 0;
             ssize_t nwritten = sendfile(tofd, fromfd, &offset, total);
             return nwritten;
         ]
     )],
     [
         AC_MSG_RESULT([yes])
         AC_DEFINE([HAVE_LINUX_SENDFILE_API], [1], [Whether linux sendfile() API is available])
     ],
     [
         AC_MSG_RESULT([no])
     ])

AC_MSG_CHECKING([for darwin sendfile support])
AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
         [
             #include <stdlib.h>
             #include <sys/types.h>
             #include <sys/socket.h>
             #include <sys/uio.h>
         ],
         [
             int fd = 0, s = 0;
             off_t offset = 0, len;
             struct sf_hdtr *hdtr = NULL;
             int flags = 0;
             int ret;
             ret = sendfile(fd, s, offset, &len, hdtr, flags);
             return ret;
         ]
     )],
     [
         AC_MSG_RESULT([yes])
         AC_DEFINE([HAVE_DARWIN_SENDFILE_API], [1], [Whether darwin sendfile() API is available])
     ],
     [
         AC_MSG_RESULT([no])
     ])

AC_MSG_CHECKING([for freebsd sendfile support])
AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
         [
             #include <stdlib.h>
             #include <sys/types.h>
             #include <sys/socket.h>
             #include <sys/uio.h>
         ],
         [
             int fromfd=0, tofd=0, ret, total=0;
             off_t offset=0, nwritten;
             struct sf_hdtr hdr;
             struct iovec hdtrl;
             hdr.headers = &hdtrl;
             hdr.hdr_cnt = 1;
             hdr.trailers = NULL;
             hdr.trl_cnt = 0;
             hdtrl.iov_base = NULL;
             hdtrl.iov_len = 0;
             ret = sendfile(fromfd, tofd, offset, total, &hdr, &nwritten, 0);
         ]
     )],
     [
         AC_MSG_RESULT([yes])
         AC_DEFINE([HAVE_FREEBSD_SENDFILE_API], [1], [Whether freebsd sendfile() API is available])
     ],
     [
         AC_MSG_RESULT([no])
     ])
CFLAGS="$CFLAGS_SAVE"

LDFLAGS_SAVE="$LDFLAGS"
for dir in "" /usr/local $SEARCH_DIR; do
    if test -n "$dir"; then
        LDFLAGS="$LDFLAGS -L$dir/lib"
    fi
    AC_CHECK_LIB([jpeg], [jpeg_set_defaults], [LIBJPEG_LIBS="-ljpeg"], [unset ac_cv_lib_jpeg_jpeg_set_defaults; LDFLAGS="$LDFLAGS_SAVE"; continue])
    break
done
test x"$ac_cv_lib_jpeg_jpeg_set_defaults" = x"yes" || AC_MSG_ERROR([Could not find libjpeg])
AC_SUBST(LIBJPEG_LIBS)

LDFLAGS_SAVE="$LDFLAGS"
for dir in "" /usr/local $SEARCH_DIR; do
    if test -n "$dir"; then
        LDFLAGS="$LDFLAGS -L$dir/lib"
    fi
    AC_CHECK_LIB([exif], [exif_data_new_from_file], [LIBEXIF_LIBS="-lexif"], [unset ac_cv_lib_exif_exif_data_new_from_file; LDFLAGS="$LDFLAGS_SAVE"; continue])
    break
done
test x"$ac_cv_lib_jpeg_jpeg_set_defaults" = x"yes" || AC_MSG_ERROR([Could not find libexif])
AC_SUBST(LIBEXIF_LIBS)

LDFLAGS_SAVE="$LDFLAGS"
for dir in "" /usr/local $SEARCH_DIR; do
    if test -n "$dir"; then
        LDFLAGS="$LDFLAGS -L$dir/lib"
    fi
    AC_CHECK_LIB([id3tag], [id3_file_open], [LIBID3TAG_LIBS="-lid3tag"], [unset ac_cv_lib_id3tag_id3_file_open; LDFLAGS="$LDFLAGS_SAVE"; continue])
    break
done
test x"$ac_cv_lib_id3tag_id3_file_open" = x"yes" || AC_MSG_ERROR([Could not find libid3tag])
AC_SUBST(LIBID3TAG_LIBS)

LDFLAGS_SAVE="$LDFLAGS"
for dir in "" /usr/local $SEARCH_DIR; do
    if test -n "$dir"; then
        LDFLAGS="$LDFLAGS -L$dir/lib"
    fi
    AC_CHECK_LIB(sqlite3, sqlite3_open, [LIBSQLITE3_LIBS="-lsqlite3"], [unset ac_cv_lib_sqlite3_sqlite3_open; LDFLAGS="$LDFLAGS_SAVE"; continue])
    AC_CHECK_LIB(sqlite3, sqlite3_malloc, [AC_DEFINE([HAVE_SQLITE3_MALLOC], [1], [Define to 1 if the sqlite3_malloc function exists.])])
    AC_CHECK_LIB(sqlite3, sqlite3_prepare_v2, [AC_DEFINE([HAVE_SQLITE3_PREPARE_V2], [1], [Define to 1 if the sqlite3_prepare_v2 function exists.])])
    break
done
test x"$ac_cv_lib_sqlite3_sqlite3_open" = x"yes" || AC_MSG_ERROR([Could not find libsqlite3])
AC_SUBST(LIBSQLITE3_LIBS)

LDFLAGS_SAVE="$LDFLAGS"
for dir in "" /usr/local $SEARCH_DIR; do
    if test -n "$dir"; then
        LDFLAGS="$LDFLAGS -L$dir/lib"
    fi
    AC_CHECK_LIB([avformat], [av_open_input_file], [LIBAVFORMAT_LIBS="-lavformat"],
		 [AC_CHECK_LIB([avformat], [avformat_open_input], [LIBAVFORMAT_LIBS="-lavformat"],
		  [unset ac_cv_lib_avformat_av_open_input_file; unset ac_cv_lib_avformat_avformat_open_input; LDFLAGS="$LDFLAGS_SAVE"; continue])])
    break
done
if test x"$ac_cv_lib_avformat_av_open_input_file" != x"yes" &&
   test x"$ac_cv_lib_avformat_avformat_open_input" != x"yes"; then
   AC_MSG_ERROR([Could not find libavformat - part of ffmpeg])
fi
AC_SUBST(LIBAVFORMAT_LIBS)

AC_CHECK_LIB(avutil ,[av_rescale_q], [LIBAVUTIL_LIBS="-lavutil"], [AC_MSG_ERROR([Could not find libavutil - part of ffmpeg])])
AC_SUBST(LIBAVUTIL_LIBS)

AC_CHECK_LIB(pthread, pthread_create)

# test if we have vorbisfile
# prior versions had ov_open_callbacks in libvorbis, test that, too.
AC_CHECK_LIB(vorbisfile, ov_open_callbacks,
        [AC_CHECK_HEADERS([vorbis/vorbisfile.h],
         AM_CONDITIONAL(HAVE_VORBISFILE, true)
         AC_DEFINE(HAVE_VORBISFILE,1,[Have vorbisfile]),
         AM_CONDITIONAL(HAVE_VORBISFILE, false))],
         AM_CONDITIONAL(HAVE_VORBISFILE, false),
         -lvorbis -logg)
AC_CHECK_LIB(FLAC, FLAC__stream_decoder_init_stream,
        [AC_CHECK_HEADERS([FLAC/all.h],
         AM_CONDITIONAL(HAVE_FLAC, true)
         AC_DEFINE(HAVE_FLAC,1,[Have flac]),
         AM_CONDITIONAL(HAVE_FLAC, false))],
         AM_CONDITIONAL(HAVE_FLAC, false),
        -logg)
# test without -logg to see whether we really need it (libflac can be without)
AC_CHECK_LIB(FLAC, FLAC__stream_decoder_init_ogg_stream,
         AM_CONDITIONAL(HAVE_FLAC, true)
         AC_DEFINE(HAVE_FLAC,1,[Have flac])
         AM_CONDITIONAL(NEED_OGG, false),
        [AM_CONDITIONAL(NEED_OGG, true)])
AC_CHECK_LIB(vorbisfile, vorbis_comment_query,
        AM_CONDITIONAL(NEED_VORBIS, false),
        AM_CONDITIONAL(NEED_VORBIS, true),
        -logg)

AC_CHECK_LIB(avahi-client, avahi_threaded_poll_new,
        [AC_CHECK_HEADERS([avahi-common/thread-watch.h],
         AM_CONDITIONAL(HAVE_AVAHI, true)
         AC_DEFINE(HAVE_AVAHI,1,[Have avahi]),
         AM_CONDITIONAL(HAVE_AVAHI, false))],
         AM_CONDITIONAL(HAVE_AVAHI, false),
        -lavahi-client -lavahi-common)

################################################################################################################
### Header checks

AC_CHECK_HEADERS([arpa/inet.h asm/unistd.h endian.h machine/endian.h fcntl.h libintl.h locale.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/file.h sys/inotify.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h unistd.h])

AC_CHECK_FUNCS(inotify_init, AC_DEFINE(HAVE_INOTIFY,1,[Whether kernel has inotify support]), [
    AC_MSG_CHECKING([for __NR_inotify_init syscall])
    AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM(
             [
                 #include <asm/unistd.h>
             ],
             [
                 #ifndef __NR_inotify_init
                 #error
                 #endif
             ]
         )],
         [
             AC_MSG_RESULT([yes])
             AC_DEFINE(HAVE_INOTIFY,1,[Whether kernel has inotify support])
         ],
         [
             AC_MSG_RESULT([no])
         ])
])

################################################################################################################
### Build Options

AC_ARG_WITH(log-path,
	AS_HELP_STRING([--with-log-path=PATH],[Default log path]),
	[with_logpath="$withval"],[with_logpath="/var/log"])
AC_DEFINE_UNQUOTED([DEFAULT_LOG_PATH],"${with_logpath}",[Log path])


AC_ARG_WITH(db-path,
	AS_HELP_STRING([--with-db-path=PATH],[Default DB path]),
	[with_dbpath="$withval"],[with_dbpath="/var/cache/minidlna"])
AC_DEFINE_UNQUOTED([DEFAULT_DB_PATH],"${with_dbpath}",[DB path])

AC_ARG_WITH(os-name,
	AS_HELP_STRING([--with-os-name=NAME],[OS Name]),
	[with_osname="$withval"],[with_osname="$(uname -s)"])
AC_DEFINE_UNQUOTED([OS_NAME],"${with_osname}",[OS Name])

AC_ARG_WITH(os-version,
	AS_HELP_STRING([--with-os-version=VERS],[OS Version]),
	[with_osver="$withval"],[with_osver="$(uname -r)"])
AC_DEFINE_UNQUOTED([OS_VERSION],"${with_osver}",[OS Version])

AC_ARG_WITH(os-url,
	AS_HELP_STRING([--with-os-url=URL],[OS URL]),
	[with_osurl="$withval"],[with_osurl="http://www.netgear.com"])
AC_DEFINE_UNQUOTED([OS_URL],"${with_osurl}",[OS URL])


AC_MSG_CHECKING([whether to enable TiVo support])
AC_ARG_ENABLE(tivo,
	[  --enable-tivo           enable TiVo support],[
	if test "$enableval" = "yes"; then
		AC_DEFINE(TIVO_SUPPORT, 1, [Define to 1 if you want to enable TiVo support])
		AM_CONDITIONAL(TIVO_SUPPORT, true)
		AC_MSG_RESULT([yes])
	else
		AM_CONDITIONAL(TIVO_SUPPORT, false)
		AC_MSG_RESULT([no])
	fi
	],[
		AM_CONDITIONAL(TIVO_SUPPORT, false)
		AC_MSG_RESULT([no])
	]
)

AC_MSG_CHECKING([whether to enable generic NETGEAR device support])
AC_ARG_ENABLE(netgear,
	[  --enable-netgear        enable generic NETGEAR device support],[
	if test "$enableval" = "yes"; then
		AC_DEFINE([NETGEAR],[1],[Define to 1 if you want to enable generic NETGEAR device support])
		AC_DEFINE_UNQUOTED([OS_URL],"http://www.netgear.com/")
		AC_DEFINE_UNQUOTED([ROOTDEV_MANUFACTURERURL],"http://www.netgear.com/")
		AC_DEFINE_UNQUOTED([ROOTDEV_MANUFACTURER],"NETGEAR")
		AC_DEFINE_UNQUOTED([ROOTDEV_MODELNAME],"Windows Media Connect compatible (ReadyDLNA)")
		AC_DEFINE_UNQUOTED([ROOTDEV_MODELDESCRIPTION],"ReadyDLNA")
                AC_MSG_RESULT([yes])
        else
                AC_MSG_RESULT([no])
        fi
        ],[
                AC_MSG_RESULT([no])
        ]
)

AC_MSG_CHECKING([whether to enable NETGEAR ReadyNAS support])
AC_ARG_ENABLE(readynas,
	[  --enable-readynas       enable NETGEAR ReadyNAS support],[
	if test "$enableval" = "yes"; then
		AC_DEFINE([NETGEAR],[1],[Define to 1 if you want to enable generic NETGEAR device support])
		AC_DEFINE([READYNAS],[1],[Define to 1 if you want to enable NETGEAR ReadyNAS support])
		AC_DEFINE([TIVO_SUPPORT], 1, [Define to 1 if you want to enable TiVo support])
		AC_DEFINE_UNQUOTED([OS_URL],"http://www.readynas.com/")
		AC_DEFINE_UNQUOTED([ROOTDEV_MANUFACTURERURL],"http://www.netgear.com/")
		AC_DEFINE_UNQUOTED([ROOTDEV_MANUFACTURER],"NETGEAR")
		AC_DEFINE_UNQUOTED([ROOTDEV_MODELNAME],"Windows Media Connect compatible (ReadyDLNA)")
		AC_DEFINE_UNQUOTED([ROOTDEV_MODELDESCRIPTION],"ReadyDLNA")
		AM_CONDITIONAL(TIVO_SUPPORT, true)
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
	],[
		AC_MSG_RESULT([no])
	]
)

AC_MSG_CHECKING([whether to build a static binary executable])
AC_ARG_ENABLE(static,
	[  --enable-static         build a static binary executable],[
	if test "$enableval" = "yes"; then
		STATIC_CFLAGS="-DSTATIC"
		AC_SUBST(STATIC_CFLAGS)
		STATIC_LDFLAGS="-Wl,-Bstatic"
		AC_SUBST(STATIC_LDFLAGS)
                AC_MSG_RESULT([yes])
        else
                AC_MSG_RESULT([no])
        fi
        ],[
                AC_MSG_RESULT([no])
        ]
)


case "$target_os" in
	darwin*)
		;;
        freebsd*)
                VER=`grep '#define __FreeBSD_version' /usr/include/sys/param.h | awk '{print $3}'`
                OS_URL=http://www.freebsd.org/
                ;;
	solaris*)
		AC_DEFINE([USE_IPF], [1], [Define to enable IPF])
		AC_DEFINE([LOG_PERROR], [0], [Define to enable logging])
		AC_DEFINE([SOLARIS_KSTATS], [1], [Define to enable Solaris Kernel Stats])
		;;
	kfreebsd*)
                OS_URL=http://www.debian.org/
		;;
        linux*)
		;;
        openbsd*)
                OS_URL=http://www.openbsd.org/
                ;;
        *)
                echo "Unknown OS : $target_os"
                ;;
esac


AC_OUTPUT([ po/Makefile.in
Makefile
])
