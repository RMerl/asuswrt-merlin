dnl *************************** libblkid needs version 1.40 or later ***********************
AC_DEFUN([AC_BLKID_VERS], [
  AC_MSG_CHECKING(for suitable libblkid version)
  AC_CACHE_VAL([libblkid_cv_is_recent],
   [
    saved_LIBS="$LIBS"
    LIBS=-lblkid
    AC_TRY_RUN([
	#include <blkid/blkid.h>
	int main()
	{
		int vers = blkid_get_library_version(0, 0);
		return vers >= 140 ? 0 : 1;
	}
       ], [libblkid_cv_is_recent=yes], [libblkid_cv_is_recent=no],
       [libblkid_cv_is_recent=unknown])
    LIBS="$saved_LIBS"])
  AC_MSG_RESULT($libblkid_cv_is_recent)
])dnl
