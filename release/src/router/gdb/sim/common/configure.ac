dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)
AC_CONFIG_HEADER(cconfig.h:config.in)

# Bugs in autoconf 2.59 break the call to SIM_AC_COMMON, hack around
# it by inlining the macro's contents.
sinclude(../common/common.m4)

# Put a useful copy of CPP_FOR_TARGET in Makefile.
# This is only used to build the target values header files.  These files are
# shipped with distributions so CPP_FOR_TARGET only needs to work in
# developer's trees.  This value is borrowed from ../../Makefile.in.
CPP_FOR_TARGET="\` \
  if test -f \$\${rootme}/../../gcc/Makefile ; then \
    if test -f \$\${rootme}/../../\$(TARGET_SUBDIR)/newlib/Makefile ; then \
      echo \$\${rootme}/../../gcc/xgcc -B\$\${rootme}/../../gcc/ -idirafter \$\${rootme}/../../\$(TARGET_SUBDIR)/newlib/targ-include -idirafter \$(srcroot)/newlib/libc/include -nostdinc; \
    else \
      echo \$\${rootme}/../../gcc/xgcc -B\$\${rootme}/../../gcc/; \
    fi; \
  else \
    if test '\$(host_canonical)' = '\$(target_canonical)' ; then \
      echo \$(CC); \
    else \
      t='\$(program_transform_name)'; echo gcc | sed -e 's/x/x/' \$\$t; \
    fi; \
  fi\` -E"
AC_SUBST(CPP_FOR_TARGET)

# Set TARGET_SUBDIR, needed by CPP_FOR_TARGET.
if test x"${host}" = x"${target}" ; then
	TARGET_SUBDIR="."
else
	TARGET_SUBDIR=${target_alias}
fi
AC_SUBST(TARGET_SUBDIR)

# These aren't all needed yet, but will be eventually.
AC_CHECK_HEADERS(stdlib.h string.h strings.h time.h sys/times.h sys/stat.h sys/mman.h)
AC_CHECK_FUNCS(mmap munmap lstat truncate ftruncate)
SIM_CHECK_MEMBERS([[struct stat.st_dev], [struct stat.st_ino],
[struct stat.st_mode], [struct stat.st_nlink], [struct stat.st_uid],
[struct stat.st_gid], [struct stat.st_rdev], [struct stat.st_size],
[struct stat.st_blksize], [struct stat.st_blocks], [struct stat.st_atime],
[struct stat.st_mtime], [struct stat.st_ctime]])
AC_OUTPUT(Makefile,
[case x$CONFIG_HEADERS in xcconfig.h:config.in) echo > stamp-h ;; esac])
