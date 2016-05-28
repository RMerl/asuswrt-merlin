AC_CHECK_HEADERS(direct.h windows.h winsock2.h ws2tcpip.h)

#######################################
# Check for mkdir mode
AC_CACHE_CHECK( [whether mkdir supports mode], ac_mkdir_has_mode,
	AC_TRY_COMPILE([
		#include <stdio.h>
		#ifdef HAVE_DIRECT_H
		#include <direct.h>
		#endif],[
			mkdir("foo",0777);
			return 0;
	],
    ac_mkdir_has_mode="yes",
    ac_mkdir_has_mode="no") )

if test "$ac_mkdir_has_mode" = "yes"
then
    AC_DEFINE(HAVE_MKDIR_MODE, 1, [Define if target mkdir supports mode option])
fi
