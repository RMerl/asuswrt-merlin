dnl Removes -I/usr/include/? from given variable
AC_DEFUN([CFLAGS_REMOVE_USR_INCLUDE],[
  ac_new_flags=""
  for i in [$]$1; do
    case [$]i in
    -I/usr/include|-I/usr/include/) ;;
    *) ac_new_flags="[$]ac_new_flags [$]i" ;;
    esac
  done
  $1=[$]ac_new_flags
])

dnl Removes -L/usr/lib/? from given variable
AC_DEFUN([LIB_REMOVE_USR_LIB],[
  ac_new_flags=""
  for i in [$]$1; do
    case [$]i in
    -L/usr/lib|-L/usr/lib/|-L/usr|-L/usr/) ;;
    *) ac_new_flags="[$]ac_new_flags [$]i" ;;
    esac
  done
  $1=[$]ac_new_flags
])

