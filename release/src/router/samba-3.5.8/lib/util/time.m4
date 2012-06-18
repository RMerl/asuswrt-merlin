AC_CACHE_CHECK([if gettimeofday takes tz argument],samba_cv_HAVE_GETTIMEOFDAY_TZ,[
AC_TRY_RUN([
#include <sys/time.h>
#include <unistd.h>
main() { struct timeval tv; exit(gettimeofday(&tv, NULL));}],
           samba_cv_HAVE_GETTIMEOFDAY_TZ=yes,samba_cv_HAVE_GETTIMEOFDAY_TZ=no,samba_cv_HAVE_GETTIMEOFDAY_TZ=yes)])
if test x"$samba_cv_HAVE_GETTIMEOFDAY_TZ" = x"yes"; then
    AC_DEFINE(HAVE_GETTIMEOFDAY_TZ,1,[Whether gettimeofday() is available])
fi
