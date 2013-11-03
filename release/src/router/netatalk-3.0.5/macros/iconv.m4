AC_DEFUN([AC_NETATALK_CHECK_ICONV],
[

dnl	#################################################
dnl	# check for libiconv support
	saved_CPPFLAGS="$CPPFLAGS"
        savedcflags="$CFLAGS"
        savedldflags="$LDFLAGS"
	ICONV_CFLAGS=""
	ICONV_LIBS=""

	AC_ARG_WITH(libiconv,
	[  --with-libiconv=BASEDIR Use libiconv in BASEDIR/lib and BASEDIR/include [[default=auto]]],
	[ case "$withval" in
	  no)
	    ;;
	  yes)
	    ;;
	  *)
	    ICONV_CFLAGS="-I$withval/include"
	    ICONV_LIBS="-L$withval/$atalk_libname"
	    ;;
	  esac ],
	  withval="no"
	)	

	CFLAGS="$ICONV_CFLAGS $CFLAGS"
        LDFLAGS="$LDFLAGS $ICONV_LIBS -liconv"

	AC_CACHE_CHECK([for libiconv],netatalk_cv_iconv,[
          AC_TRY_LINK([
#include <stdlib.h>
#include <iconv.h>
],[
	iconv_t cd = iconv_open("","");
        iconv(cd,NULL,NULL,NULL,NULL);
        iconv_close(cd);
], netatalk_cv_iconv=yes, netatalk_cv_iconv=no, netatalk_cv_iconv=cross)])

	if test x"$netatalk_cv_iconv" = x"yes"; then
	    ICONV_LIBS="$ICONV_LIBS -liconv"
        else
dnl	    # unset C-/LDFLAGS so we can detect glibc iconv, if available
	    CFLAGS="$savedcflags"
	    LDFLAGS="$savedldflags"
	    ICONV_LIBS=""
	    ICONV_CFLAGS=""
	    if test x"$withval" != x"no"; then
	        AC_MSG_ERROR([libiconv not found])
	    fi
	fi


	CFLAGS_REMOVE_USR_INCLUDE(ICONV_CFLAGS)
	LIB_REMOVE_USR_LIB(ICONV_LIBS)
	AC_SUBST(ICONV_CFLAGS)
	AC_SUBST(ICONV_LIBS)

dnl	############
dnl	# check for iconv usability

	AC_CACHE_CHECK([for working iconv],netatalk_cv_HAVE_USABLE_ICONV,[
		AC_TRY_RUN([\
#include <iconv.h>
main() {
       iconv_t cd = iconv_open("ASCII", "UTF-8");
       if (cd == 0 || cd == (iconv_t)-1) return -1;
       return 0;
}
], netatalk_cv_HAVE_USABLE_ICONV=yes,netatalk_cv_HAVE_USABLE_ICONV=no,netatalk_cv_HAVE_USABLE_ICONV=cross)])

	if test x"$netatalk_cv_HAVE_USABLE_ICONV" = x"yes"; then
	    AC_DEFINE(HAVE_USABLE_ICONV,1,[Whether to use native iconv])
	fi

dnl	###########
dnl	# check if iconv needs const
  	if test x"$netatalk_cv_HAVE_USABLE_ICONV" = x"yes"; then
    		AC_CACHE_VAL(am_cv_proto_iconv, [
      		AC_TRY_COMPILE([\
#include <stdlib.h>
#include <iconv.h>
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
], [], am_cv_proto_iconv_arg1="", am_cv_proto_iconv_arg1="const")
	      	am_cv_proto_iconv="extern size_t iconv (iconv_t cd, $am_cv_proto_iconv_arg1 char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);"])
    		AC_DEFINE_UNQUOTED(ICONV_CONST, $am_cv_proto_iconv_arg1,
      			[Define as const if the declaration of iconv() needs const.])
  	fi

dnl     ###########
dnl     # check if (lib)iconv supports UCS-2-INTERNAL
	if test x"$netatalk_cv_HAVE_USABLE_ICONV" = x"yes"; then
	    AC_CACHE_CHECK([whether iconv supports UCS-2-INTERNAL],netatalk_cv_HAVE_UCS2INTERNAL,[
		AC_TRY_RUN([\
#include <iconv.h>
int main() {
       iconv_t cd = iconv_open("ASCII", "UCS-2-INTERNAL");
       if (cd == 0 || cd == (iconv_t)-1) return -1;
       return 0;
}
], netatalk_cv_HAVE_UCS2INTERNAL=yes,netatalk_cv_HAVE_UCS2INTERNAL=no,netatalk_cv_HAVEUCS2INTERNAL=cross)])

	if test x"$netatalk_cv_HAVE_UCS2INTERNAL" = x"yes"; then
		AC_DEFINE(HAVE_UCS2INTERNAL,1,[Whether UCS-2-INTERNAL is supported])
	fi
	fi

        CFLAGS="$savedcflags"
        LDFLAGS="$savedldflags"
	CPPFLAGS="$saved_CPPFLAGS"
	
])
