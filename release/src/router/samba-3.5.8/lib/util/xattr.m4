dnl ############################################
dnl use flistxattr as the key function for having 
dnl sufficient xattr support for posix xattr backend
AC_CHECK_HEADERS(sys/attributes.h attr/xattr.h sys/xattr.h)
AC_SEARCH_LIBS_EXT(flistxattr, [attr], XATTR_LIBS)
AC_CHECK_FUNC_EXT(flistxattr, $XATTR_LIBS)
SMB_EXT_LIB(XATTR,[${XATTR_LIBS}],[${XATTR_CFLAGS}],[${XATTR_CPPFLAGS}],[${XATTR_LDFLAGS}])
if test x"$ac_cv_func_ext_flistxattr" = x"yes"; then
	AC_CACHE_CHECK([whether xattr interface takes additional options], smb_attr_cv_xattr_add_opt,
	[old_LIBS=$LIBS
	 LIBS="$LIBS $XATTRLIBS"
	 AC_TRY_COMPILE([
	 	#include <sys/types.h>
		#if HAVE_ATTR_XATTR_H
		#include <attr/xattr.h>
		#elif HAVE_SYS_XATTR_H
		#include <sys/xattr.h>
		#endif
		#ifndef NULL
		#define NULL ((void *)0)
		#endif
		],[
		getxattr(NULL, NULL, NULL, 0, 0, 0);
		],smb_attr_cv_xattr_add_opt=yes,smb_attr_cv_xattr_add_opt=no)
	  LIBS=$old_LIBS])
	if test x"$smb_attr_cv_xattr_add_opt" = x"yes"; then
		AC_DEFINE(XATTR_ADDITIONAL_OPTIONS, 1, [xattr functions have additional options])
	fi
	AC_DEFINE(HAVE_XATTR_SUPPORT,1,[Whether we have xattr support])
	SMB_ENABLE(XATTR,YES)
fi

