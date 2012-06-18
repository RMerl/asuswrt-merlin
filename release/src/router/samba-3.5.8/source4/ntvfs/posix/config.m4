

dnl #############################################
dnl see if we have nanosecond resolution for stat
AC_CACHE_CHECK([for tv_nsec nanosecond fields in struct stat],ac_cv_have_stat_tv_nsec,[
AC_TRY_COMPILE(
[
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
],
[struct stat st; 
 st.st_mtim.tv_nsec;
 st.st_atim.tv_nsec;
 st.st_ctim.tv_nsec;
],
ac_cv_decl_have_stat_tv_nsec=yes,
ac_cv_decl_have_stat_tv_nsec=no)
])
if test x"$ac_cv_decl_have_stat_tv_nsec" = x"yes"; then
   AC_DEFINE(HAVE_STAT_TV_NSEC,1,[Whether stat has tv_nsec nanosecond fields])
fi

AC_CHECK_HEADERS(blkid/blkid.h)
AC_SEARCH_LIBS_EXT(blkid_get_cache, [blkid], BLKID_LIBS)
AC_CHECK_FUNC_EXT(blkid_get_cache, $BLKID_LIBS)
SMB_EXT_LIB(BLKID,[${BLKID_LIBS}],[${BLKID_CFLAGS}],[${BLKID_CPPFLAGS}],[${BLKID_LDFLAGS}])
if test x"$ac_cv_func_ext_blkid_get_cache" = x"yes"; then
	AC_DEFINE(HAVE_LIBBLKID,1,[Whether we have blkid support (e2fsprogs)])
	SMB_ENABLE(BLKID,YES)
fi

SMB_ENABLE(pvfs_aio,NO)
if test x"$tevent_cv_aio_support" = x"yes"; then
	SMB_ENABLE(pvfs_aio,YES)
fi
