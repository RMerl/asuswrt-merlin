dnl -------------------------------------------------------------------
dnl This test for largefile support was written by Vadim Zeitlin for 
dnl wxWindows. 
dnl -------------------------------------------------------------------

dnl WX_SYS_LARGEFILE_TEST
dnl
dnl NB: original autoconf test was checking if compiler supported 6 bit off_t
dnl     arithmetic properly but this failed miserably with gcc under Linux
dnl     whereas the system still supports 64 bit files, so now simply check
dnl     that off_t is big enough
define(WX_SYS_LARGEFILE_TEST,
[typedef struct {
    unsigned int field: sizeof(off_t) == 8;
} wxlf;
])

dnl WX_SYS_LARGEFILE_MACRO_VALUE(C-MACRO, VALUE, CACHE-VAR)
define(WX_SYS_LARGEFILE_MACRO_VALUE,
[
    AC_CACHE_CHECK([for $1 value needed for large files], [$3],
        [
          AC_TRY_COMPILE([#define $1 $2
                          #include <sys/types.h>],
                         WX_SYS_LARGEFILE_TEST,
                         [$3=$2],
                         [$3=no])
        ]
    )

    if test "$$3" != no; then
        wx_largefile=yes
        AC_DEFINE_UNQUOTED([$1], [$$3], [$1 (for LARGEFILE support)])
    fi
])




dnl AC_SYS_LARGEFILE
dnl ----------------
dnl By default, many hosts won't let programs access large files;
dnl one must use special compiler options to get large-file access to work.
dnl For more details about this brain damage please see:
dnl http://www.sas.com/standards/large.file/x_open.20Mar96.html
AC_DEFUN([AC_SYS_LARGEFILE],
[AC_ARG_ENABLE(largefile,
               [  --disable-largefile     omit support for large files])
if test "$enable_largefile" != no; then
    dnl _FILE_OFFSET_BITS==64 is needed for Linux, Solaris, ...
    dnl _LARGE_FILES -- for AIX
    wx_largefile=no
    WX_SYS_LARGEFILE_MACRO_VALUE(_FILE_OFFSET_BITS, 64, ac_cv_sys_file_offset_bits) 
    if test "x$wx_largefile" != "xyes"; then
        WX_SYS_LARGEFILE_MACRO_VALUE(_LARGE_FILES, 1, ac_cv_sys_large_files)
    fi

    
    AC_CACHE_CHECK([for 64 bit off_t],netatalk_cv_SIZEOF_OFF_T,[
    AC_TRY_RUN([#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
main() { exit((sizeof(off_t) == 8) ? 0 : 1); }],
netatalk_cv_SIZEOF_OFF_T=yes,netatalk_cv_SIZEOF_OFF_T=no,netatalk_cv_SIZEOF_OFF_T=cross)])

    AC_MSG_CHECKING([if large file support is available])
    if test "x$netatalk_cv_SIZEOF_OFF_T" != "xno"; then
        AC_DEFINE(HAVE_LARGEFILE_SUPPORT, [], [LARGEFILE support])
	AC_MSG_RESULT([yes])
        ifelse([$1], , :, [$1])
    else
        AC_MSG_RESULT([no])
        ifelse([$2], , :, [$2])
    fi
fi
])

dnl ----------end of largefile test------------------------------------
