dnl Checks for libnfsidmap
dnl
AC_DEFUN([AC_LIBNFSIDMAP], [

  dnl Check for libnfsidmap, but do not add -lnfsidmap to LIBS
  AC_CHECK_LIB([nfsidmap], [nfs4_init_name_mapping], [libnfsidmap=1],
               [AC_MSG_ERROR([libnfsidmap not found.])])

  AC_CHECK_HEADERS([nfsidmap.h], ,
                   [AC_MSG_ERROR([libnfsidmap headers not found.])])

  dnl nfs4_set_debug() doesn't appear in all versions of libnfsidmap
  AC_CHECK_LIB([nfsidmap], [nfs4_set_debug],
               [AC_DEFINE([HAVE_NFS4_SET_DEBUG], 1,
                          [Define to 1 if you have the `nfs4_set_debug' function.])])

])dnl
