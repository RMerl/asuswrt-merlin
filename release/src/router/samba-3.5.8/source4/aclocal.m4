
dnl Copied from libtool.m4
AC_DEFUN(AC_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

dnl Removes -I/usr/include/? from given variable
AC_DEFUN(CFLAGS_REMOVE_USR_INCLUDE,[
  ac_new_flags=""
  for i in [$]$1; do
    case [$]i in
    -I/usr/include|-I/usr/include/) ;;
    *) ac_new_flags="[$]ac_new_flags [$]i" ;;
    esac
  done
  $1=[$]ac_new_flags
])
    
dnl Removes '-L/usr/lib[/]', '-Wl,-rpath,/usr/lib[/]'
dnl and '-Wl,-rpath -Wl,/usr/lib[/]' from given variable
AC_DEFUN(LIB_REMOVE_USR_LIB,[
  ac_new_flags=""
  l=""
  for i in [$]$1; do
    case [$]l[$]i in
    -L/usr/lib) ;;
    -L/usr/lib/) ;;
    -L/usr/lib64) ;;
    -L/usr/lib64/) ;;
    -Wl,-rpath,/usr/lib) l="";;
    -Wl,-rpath,/usr/lib/) l="";;
    -Wl,-rpath,/usr/lib64) l="";;
    -Wl,-rpath,/usr/lib64/) l="";;
    -Wl,-rpath) l=[$]i;;
    -Wl,-rpath-Wl,/usr/lib) l="";;
    -Wl,-rpath-Wl,/usr/lib/) l="";;
    -Wl,-rpath-Wl,/usr/lib64) l="";;
    -Wl,-rpath-Wl,/usr/lib64/) l="";;
    *)
    	s=" "
        if test x"[$]ac_new_flags" = x""; then
            s="";
	fi
        if test x"[$]l" = x""; then
            ac_new_flags="[$]ac_new_flags[$]s[$]i";
        else
            ac_new_flags="[$]ac_new_flags[$]s[$]l [$]i";
        fi
        l=""
        ;;
    esac
  done
  $1=[$]ac_new_flags
])

m4_include(../lib/replace/libreplace.m4)
m4_include(build/m4/ax_cflags_gcc_option.m4)
m4_include(build/m4/ax_cflags_irix_option.m4)
m4_include(build/m4/public.m4)
