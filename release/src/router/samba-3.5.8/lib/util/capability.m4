AC_CACHE_CHECK([for irix specific capabilities],samba_cv_HAVE_IRIX_SPECIFIC_CAPABILITIES,[
AC_TRY_RUN([#include <sys/types.h>
#include <sys/capability.h>
main() {
 cap_t cap;
 if ((cap = cap_get_proc()) == NULL)
   exit(1);
 cap->cap_effective |= CAP_NETWORK_MGT;
 cap->cap_inheritable |= CAP_NETWORK_MGT;
 cap_set_proc(cap);
 exit(0);
}
],
samba_cv_HAVE_IRIX_SPECIFIC_CAPABILITIES=yes,samba_cv_HAVE_IRIX_SPECIFIC_CAPABILITIES=no,samba_cv_HAVE_IRIX_SPECIFIC_CAPABILITIES=cross)])
if test x"$samba_cv_HAVE_IRIX_SPECIFIC_CAPABILITIES" = x"yes"; then
    AC_DEFINE(HAVE_IRIX_SPECIFIC_CAPABILITIES,1,[Whether IRIX specific capabilities are available])
fi
