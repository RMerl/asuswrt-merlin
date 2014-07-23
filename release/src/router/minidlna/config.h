/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

#include "osver.h"

/* DB path */
#define DEFAULT_DB_PATH "/tmp/minidlna"

/* Log path */
#define DEFAULT_LOG_PATH "/tmp/minidlna"

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#ifndef HAVE_ARPA_INET_H
#define HAVE_ARPA_INET_H 1
#endif

/* Define to 1 if you have the <asm/unistd.h> header file. */
#define HAVE_ASM_UNISTD_H 1

/* Define to 1 if you have the <avcodec.h> header file. */
/* #undef HAVE_AVCODEC_H */

/* Define to 1 if you have the <avformat.h> header file. */
/* #undef HAVE_AVFORMAT_H */

/* Define to 1 if you have the <avutil.h> header file. */
/* #undef HAVE_AVUTIL_H */

/* Define to 1 if you have the MacOS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the MacOS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* use clock_gettime */
/* #undef HAVE_CLOCK_GETTIME */

/* Whether the __NR_clock_gettime syscall is defined */
#define HAVE_CLOCK_GETTIME_SYSCALL 1

/* Whether darwin sendfile() API is available */
/* #undef HAVE_DARWIN_SENDFILE_API */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <ffmpeg/avcodec.h> header file. */
/* #undef HAVE_FFMPEG_AVCODEC_H */

/* Define to 1 if you have the <ffmpeg/avformat.h> header file. */
/* #undef HAVE_FFMPEG_AVFORMAT_H */

/* Define to 1 if you have the <ffmpeg/avutil.h> header file. */
/* #undef HAVE_FFMPEG_AVUTIL_H */

/* Define to 1 if you have the <ffmpeg/libavcodec/avcodec.h> header file. */
/* #undef HAVE_FFMPEG_LIBAVCODEC_AVCODEC_H */

/* Define to 1 if you have the <ffmpeg/libavformat/avformat.h> header file. */
/* #undef HAVE_FFMPEG_LIBAVFORMAT_AVFORMAT_H */

/* Define to 1 if you have the <ffmpeg/libavutil/avutil.h> header file. */
/* #undef HAVE_FFMPEG_LIBAVUTIL_AVUTIL_H */

/* Have flac */
#define HAVE_FLAC 1

/* Define to 1 if you have the <FLAC/all.h> header file. */
#define HAVE_FLAC_ALL_H 1

/* Define to 1 if you have the <FLAC/metadata.h> header file. */
#define HAVE_FLAC_METADATA_H 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Whether freebsd sendfile() API is available */
/* #undef HAVE_FREEBSD_SENDFILE_API */

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getifaddrs' function. */
#define HAVE_GETIFADDRS 1

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV 1 */

/* Define to 1 if you have the <id3tag.h> header file. */
#define HAVE_ID3TAG_H 1

/* Define to 1 if you have the `inet_ntoa' function. */
#define HAVE_INET_NTOA 1

/* Whether kernel has inotify support */
#define HAVE_INOTIFY 1

/* Define to 1 if you have the `inotify_init' function. */
#define HAVE_INOTIFY_INIT 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <jpeglib.h> header file. */
#define HAVE_JPEGLIB_H 1

/* Define to 1 if you have the <libavcodec/avcodec.h> header file. */
#define HAVE_LIBAVCODEC_AVCODEC_H 1

/* Define to 1 if you have the <libavformat/avformat.h> header file. */
#define HAVE_LIBAVFORMAT_AVFORMAT_H 1

/* Define to 1 if you have the <libavutil/avutil.h> header file. */
#define HAVE_LIBAVUTIL_AVUTIL_H 1

/* Define to 1 if you have the <libav/avcodec.h> header file. */
/* #undef HAVE_LIBAV_AVCODEC_H */

/* Define to 1 if you have the <libav/avformat.h> header file. */
/* #undef HAVE_LIBAV_AVFORMAT_H */

/* Define to 1 if you have the <libav/avutil.h> header file. */
/* #undef HAVE_LIBAV_AVUTIL_H */

/* Define to 1 if you have the <libav/libavcodec/avcodec.h> header file. */
/* #undef HAVE_LIBAV_LIBAVCODEC_AVCODEC_H */

/* Define to 1 if you have the <libav/libavformat/avformat.h> header file. */
/* #undef HAVE_LIBAV_LIBAVFORMAT_AVFORMAT_H */

/* Define to 1 if you have the <libav/libavutil/avutil.h> header file. */
/* #undef HAVE_LIBAV_LIBAVUTIL_AVUTIL_H */

/* Define to 1 if you have the <libexif/exif-loader.h> header file. */
#define HAVE_LIBEXIF_EXIF_LOADER_H 1

/* Define to 1 if you have the <libintl.h> header file. */
#define HAVE_LIBINTL_H 1

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Whether linux sendfile() API is available */
#define HAVE_LINUX_SENDFILE_API 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <machine/endian.h> header file. */
/* #undef HAVE_MACHINE_ENDIAN_H */

/* Define to 1 if you have the <mach/mach_time.h> header file. */
/* #undef HAVE_MACH_MACH_TIME_H */

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Support for Linux netlink */
/* #undef HAVE_NETLINK */

/* Define to 1 if you have the <ogg/ogg.h> header file. */
#define HAVE_OGG_OGG_H 1

/* Define to 1 if you have the `realpath' function. */
#define HAVE_REALPATH 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `sendfile' function. */
#define HAVE_SENDFILE 1

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <sqlite3.h> header file. */
#define HAVE_SQLITE3_H 1

/* Define to 1 if the sqlite3_malloc function exists. */
#define HAVE_SQLITE3_MALLOC 1

/* Define to 1 if the sqlite3_prepare_v2 function exists. */
#define HAVE_SQLITE3_PREPARE_V2 1

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stddef.h> header file. */
/* #undef HAVE_STDDEF_H */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
/* #undef HAVE_STDLIB_H */

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the `strpbrk' function. */
#define HAVE_STRPBRK 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if `d_type' is a member of `struct dirent'. */
#define HAVE_STRUCT_DIRENT_D_TYPE 1

/* Support for struct ip_mreq */
/* #undef HAVE_STRUCT_IP_MREQ */

/* Support for struct ip_mreqn */
#define HAVE_STRUCT_IP_MREQN /**/

/* Define to 1 if `st_blocks' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLOCKS 1

/* Define to 1 if your `struct stat' has `st_blocks'. Deprecated, use
   `HAVE_STRUCT_STAT_ST_BLOCKS' instead. */
#define HAVE_ST_BLOCKS 1

/* Define to 1 if you have the <syscall.h> header file. */
#define HAVE_SYSCALL_H 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/inotify.h> header file. */
#define HAVE_SYS_INOTIFY_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/syscall.h> header file. */
#define HAVE_SYS_SYSCALL_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* lacking vorbisfile */
#define HAVE_VORBISFILE 1

/* Define to 1 if you have the <vorbis/codec.h> header file. */
#define HAVE_VORBIS_CODEC_H 1

/* Define to 1 if you have the <vorbis/vorbisfile.h> header file. */
#define HAVE_VORBIS_VORBISFILE_H 1

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Define to enable logging */
/* #undef LOG_PERROR */

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
#define LSTAT_FOLLOWS_SLASHED_SYMLINK 1

/* Define to 1 if you want to enable generic NETGEAR device support */
/* #undef NETGEAR */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* OS Name */
#define OS_NAME "asuswrt"

/* OS URL */
#define OS_URL "http://www.asus.com/"

/* Name of package */
#define PACKAGE "minidlna"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "MiniDLNA"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "MiniDLNA 1.1.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "minidlna"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.1.3"

/* Define to 5 if you want to enable NETGEAR ReadyNAS PnP-X support */
/* #undef PNPX */

/* Define to 1 if you want to enable NETGEAR ReadyNAS support */
/* #undef READYNAS */

/* scandir needs const char cast */
#define SCANDIR_CONST 1

/* we are on solaris */
/* #undef SOLARIS */

/* Define to enable Solaris Kernel Stats */
/* #undef SOLARIS_KSTATS */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you want to enable TiVo support */
#define TIVO_SUPPORT 1

/* use the system's builtin daemon() */
/* #undef USE_DAEMON */

/* Define to enable IPF */
/* #undef USE_IPF */

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "1.1.3"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define for Solaris 2.5.1 so the uint32_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT32_T */

/* Define for Solaris 2.5.1 so the uint64_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT64_T */

/* Define for Solaris 2.5.1 so the uint8_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT8_T */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to the type of a signed integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int32_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define to the type of an unsigned integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint32_t */

/* Define to the type of an unsigned integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint64_t */

/* Define to the type of an unsigned integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint8_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */
