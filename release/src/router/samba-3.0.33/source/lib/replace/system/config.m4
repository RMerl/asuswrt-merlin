# filesys
AC_HEADER_DIRENT 
AC_CHECK_HEADERS(fcntl.h sys/fcntl.h sys/resource.h sys/ioctl.h sys/mode.h sys/filio.h sys/fs/s5param.h sys/filsys.h)
AC_CHECK_HEADERS(sys/acl.h acl/libacl.h)

# select
AC_CHECK_HEADERS(sys/select.h)

# time
AC_CHECK_HEADERS(sys/time.h utime.h)
AC_HEADER_TIME

# wait
AC_HEADER_SYS_WAIT

# capability
AC_CHECK_HEADERS(sys/capability.h)

# passwd
AC_CHECK_HEADERS(grp.h sys/id.h compat.h shadow.h sys/priv.h pwd.h sys/security.h)

# locale
AC_CHECK_HEADERS(ctype.h locale.h)

# glob
AC_CHECK_HEADERS(fnmatch.h)

# shmem
AC_CHECK_HEADERS(sys/ipc.h sys/mman.h sys/shm.h )

# terminal
AC_CHECK_HEADERS(termios.h termio.h sys/termio.h )
