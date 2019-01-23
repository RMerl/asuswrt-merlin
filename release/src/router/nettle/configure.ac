dnl -*- mode: shell-script; sh-indentation: 2; -*-

dnl Process this file with autoconf to produce a configure script.

AC_INIT([nettle], [3.2], [nettle-bugs@lists.lysator.liu.se])
AC_PREREQ(2.61)
AC_CONFIG_SRCDIR([arcfour.c])
# Needed to stop autoconf from looking for files in parent directories.
AC_CONFIG_AUX_DIR([.])

AC_CONFIG_HEADER([config.h])

LIBNETTLE_MAJOR=6
LIBNETTLE_MINOR=2

LIBHOGWEED_MAJOR=4
LIBHOGWEED_MINOR=2

dnl Note double square brackets, for extra m4 quoting.
MAJOR_VERSION=`echo $PACKAGE_VERSION | sed 's/^\([[^.]]*\)\..*/\1/'`
MINOR_VERSION=`echo $PACKAGE_VERSION | sed 's/^[[^.]]*\.\([[0-9]]*\).*/\1/'`
AC_SUBST([MAJOR_VERSION])
AC_SUBST([MINOR_VERSION])

AC_CANONICAL_HOST

# Command line options
AC_ARG_WITH(include-path,
  AC_HELP_STRING([--with-include-path], [A colon-separated list of directories to search for include files]),,
  [with_include_path=''])

if test x$with_include_path != x ; then
  CPPFLAGS="$CPPFLAGS -I`echo $with_include_path | sed 's/:/ -I/g'`"
fi

AC_ARG_WITH(lib-path,
  AC_HELP_STRING([--with-lib-path], [A colon-separated list of directories to search for libraries]),,
  [with_lib_path=''])

if test x$with_lib_path != x ; then
  LDFLAGS="$LDFLAGS -L`echo $with_lib_path | sed 's/:/ -L/g'`"
fi

AC_ARG_ENABLE(public-key,
  AC_HELP_STRING([--disable-public-key], [Disable public key algorithms]),,
  [enable_public_key=yes])

AC_ARG_ENABLE(assembler,
  AC_HELP_STRING([--disable-assembler],[Disable assembler code]),,
  [enable_assembler=yes])

AC_ARG_ENABLE(static,
  AC_HELP_STRING([--disable-static], [Do not build any static library]),,
  [enable_static=yes])

AC_ARG_ENABLE(shared,
  AC_HELP_STRING([--disable-shared], [Do not build any shared library]),,
  [enable_shared=yes])

AC_ARG_ENABLE(pic,
  AC_HELP_STRING([--disable-pic],
  [Do not try to compile library files as position independent code]),,
  [enable_pic=yes])

AC_ARG_ENABLE(openssl,
  AC_HELP_STRING([--disable-openssl], [Do not include openssl glue in the benchmark program]),,
  [enable_openssl=yes])

AC_ARG_ENABLE(gcov,
  AC_HELP_STRING([--enable-gcov], [Instrument for gcov (requires a modern gcc)]),,
  [enable_gcov=no])

AC_ARG_ENABLE(documentation,
  AC_HELP_STRING([--disable-documentation], [Omit building and installing the documentation. (default=auto)]),,
  [enable_documentation=auto])

AC_ARG_ENABLE(fat, AC_HELP_STRING([--enable-fat], [Enable fat library build (default=no)]),,
  [enable_fat=no])

AC_ARG_ENABLE(arm-neon,
  AC_HELP_STRING([--enable-arm-neon], [Enable ARM Neon assembly. (default=auto)]),,
  [enable_arm_neon=auto])

AC_ARG_ENABLE(x86-aesni,
  AC_HELP_STRING([--enable-x86-aesni], [Enable x86_64 aes instructions. (default=no)]),,
  [enable_x86_aesni=no])

AC_ARG_ENABLE(mini-gmp,
  AC_HELP_STRING([--enable-mini-gmp], [Enable mini-gmp, used instead of libgmp.]),,
  [enable_mini_gmp=no])

if test "x$enable_mini_gmp" = xyes ; then
  NETTLE_USE_MINI_GMP=1
  HOGWEED_EXTRA_SYMBOLS="mpz_*;gmp_*;mpn_*;"
else
  NETTLE_USE_MINI_GMP=0
  HOGWEED_EXTRA_SYMBOLS=""
fi
AC_SUBST([NETTLE_USE_MINI_GMP])
AC_SUBST([HOGWEED_EXTRA_SYMBOLS])

LSH_RPATH_INIT([`echo $with_lib_path | sed 's/:/ /g'` \
    `echo $exec_prefix | sed "s@^NONE@$prefix/lib@g" | sed "s@^NONE@$ac_default_prefix/lib@g"` \
    /usr/local/lib /sw/local/lib /sw/lib \
    /usr/gnu/lib /opt/gnu/lib /sw/gnu/lib /usr/freeware/lib /usr/pkg/lib])

# Checks for programs.
AC_PROG_CC

NETTLE_CHECK_IFUNC

# When $CC foo.c -o foo creates both foo and foo.exe, autoconf picks
# up the foo.exe and sets exeext to .exe. That is correct for cygwin,
# which has some kind of magic link from foo to foo.exe, but not for
# rntcl. A better check for the cygwin case would check if the
# contents of foo and foo.exe are equal; in the rntcl case, foo is a
# sh script, and foo.exe is a windows executable.

if test "x$CC" = xrntcl ; then
    AC_MSG_NOTICE([Compiling with rntcl; clearing EXEEXT and disabling assembler])
    ac_exeext=''
    ac_cv_exeext=''
    EXEEXT=''
    enable_assembler=no
fi

# Used by the testsuite only
AC_PROG_CXX

AC_LANG_PUSH(C++)
AC_TRY_COMPILE([],[return 0;],[IF_CXX=''], [IF_CXX='#'])
AC_SUBST([IF_CXX])
AC_LANG_POP

LD_VERSION_SCRIPT

AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_CHECK_TOOL(NM, nm, strings)
# Used only for the GNU-stack configure test.
AC_CHECK_TOOL(OBJDUMP, objdump, false)
AC_CHECK_TOOL(AR, ar, false)

if test "x$ac_cv_prog_cc_stdc" = xno ; then
  AC_ERROR([the C compiler doesn't handle ANSI-C]) #'
fi

AC_PROG_INSTALL

# According to the autoconf manual, needs install-sh from
# autoconf-2.60 or automake-1.10 to avoid races.
AC_PROG_MKDIR_P

AC_PROG_LN_S

# Compiler tests for the build system
GMP_PROG_CC_FOR_BUILD
GMP_PROG_EXEEXT_FOR_BUILD

LSH_DEPENDENCY_TRACKING

if test x$enable_dependency_tracking = xyes ; then
  # Since the makefiles use include to get the dependency files, we must
  # make sure that the files exist. We generate some more files than are
  # actually needed.

  AC_CONFIG_COMMANDS([dummy-dep-files],
  [(cd "$srcdir" && find . '(' -name '*.c' -o -name '*.cxx' ')' -print) \
 | sed 's/\.cx*$//' | (while read f; do \
      test -f "$f.o.d" || echo > "$f.o.d"; \
   done)
])
fi

if test "x$enable_gcov" = "xyes"; then
  CFLAGS="$CFLAGS -ftest-coverage -fprofile-arcs"
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(size_t)

AC_CHECK_HEADERS([openssl/blowfish.h openssl/des.h openssl/cast.h openssl/aes.h openssl/ecdsa.h],,
[enable_openssl=no
 break])

# For use by the testsuite
AC_CHECK_HEADERS([valgrind/memcheck.h])
AC_CHECK_HEADERS([dlfcn.h])
AC_CHECK_LIB([dl], [dlopen],
	     [AC_DEFINE([HAVE_LIBDL], 1,
			[Define to 1 if you have dlopen (with -ldl).])])

LSH_FUNC_ALLOCA
LSH_FUNC_STRERROR
# getenv_secure is used for fat overrides,
# getline is used in the testsuite
AC_CHECK_FUNCS(secure_getenv getline)
AC_C_BIGENDIAN

LSH_GCC_ATTRIBUTES

# According to Simon Josefsson, looking for uint32_t and friends in
# sys/types.h is needed on some systems, in particular cygwin.
AX_CREATE_STDINT_H([nettle-stdint.h], [sys/types.h])

# Check for file locking. We (AC_PROG_CC?) have already checked for
# sys/types.h and unistd.h.
AC_CACHE_CHECK([for fcntl file locking],
		nettle_cv_fcntl_locking,
[AC_TRY_COMPILE([
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>
],[
int op = F_SETLKW;
struct flock fl;
],
nettle_cv_fcntl_locking=yes,
nettle_cv_fcntl_locking=no)])

AH_TEMPLATE([HAVE_FCNTL_LOCKING], [Define if fcntl file locking is available])
if test "x$nettle_cv_fcntl_locking" = "xyes" ; then
  AC_DEFINE(HAVE_FCNTL_LOCKING)
fi

# Checks for libraries
if test "x$enable_public_key" = "xyes" ; then
  if test "x$enable_mini_gmp" = "xno" ; then
    AC_CHECK_LIB(gmp, __gmpz_getlimbn,,
        [AC_MSG_WARN(
    [GNU MP not found, or not 3.1 or up, see http://gmplib.org/.
    Support for public key algorithms will be unavailable.])]
        enable_public_key=no)

    # Add -R flags needed to run programs linked with gmp
    LSH_RPATH_FIX
  fi
fi

nettle_cv_gmp_numb_bits=0
if test "x$enable_public_key" = "xyes" ; then
  # Check for gmp limb size
  if test "x$enable_mini_gmp" = "xyes" ; then
    AC_MSG_CHECKING([for mini-gmp limb size])
    # With mini-gmp, mp_limb_t is always unsigned long.
    AC_COMPUTE_INT(nettle_cv_gmp_numb_bits, [(sizeof(unsigned long) * CHAR_BIT)],
        [#include <limits.h>],
        [AC_MSG_FAILURE([cannot find value of GMP_NUMB_BITS])])

    AC_MSG_RESULT([$nettle_cv_gmp_numb_bits bits])
  else
    AC_MSG_CHECKING([for GMP limb size])
    AC_COMPUTE_INT(nettle_cv_gmp_numb_bits, [GMP_NUMB_BITS],
        [#include <gmp.h>],
        [AC_MSG_FAILURE([cannot find value of GMP_NUMB_BITS])])

    AC_MSG_RESULT([$nettle_cv_gmp_numb_bits bits])
  fi
fi

GMP_NUMB_BITS="$nettle_cv_gmp_numb_bits"
AC_SUBST([GMP_NUMB_BITS])

# Figure out ABI. Currently, configurable only by setting CFLAGS.
ABI=standard

case "$host_cpu" in
  [x86_64 | amd64])
    AC_TRY_COMPILE([
#if defined(__x86_64__) || defined(__arch64__)
#error 64-bit x86
#endif
    ], [], [
      ABI=32
    ], [
      ABI=64
    ])
    ;;
  *sparc*)
    AC_TRY_COMPILE([
#if defined(__sparcv9) || defined(__arch64__)
#error 64-bit sparc
#endif
    ], [], [
      ABI=32
    ], [
      ABI=64
    ])
    ;;
  *mips*)
    AC_TRY_COMPILE([
#if defined(__sgi) && defined(__LP64__)
#error 64-bit mips
#endif
    ], [], [
      ABI=32
    ], [
      ABI=64
    ])
    ;;
esac

if test "x$ABI" != xstandard ; then
  AC_MSG_NOTICE([Compiler uses $ABI-bit ABI. To change, set CC.])
  if test "$libdir" = '${exec_prefix}/lib' ; then
    # Try setting a better default
    case "$host_cpu:$host_os:$ABI" in
      *:solaris*:32|*:sunos*:32)
	libdir='${exec_prefix}/lib'
	;;
      *:solaris*:64|*:sunos*:64)
	libdir='${exec_prefix}/lib/64'
	;;
      # Linux conventions are a mess... According to the Linux File
      # Hierarchy Standard, all architectures except IA64 puts 32-bit
      # libraries in lib, and 64-bit in lib64. Some distributions,
      # e.g., Fedora and Gentoo, adhere to this standard, while at
      # least Debian has decided to put 64-bit libraries in lib and
      # 32-bit libraries in lib32.
      
      # We try to figure out the convention, except if we're cross
      # compiling. We use lib${ABI} if /usr/lib${ABI} exists and
      # appears to not be a symlink to a different name.
      *:linux*:32|*:linux*:64)
	if test "$cross_compiling" = yes ; then
          AC_MSG_WARN([Cross compiling for linux. Can't guess if libraries go in lib${ABI} or lib.]); dnl '
	else
	  # The dash builtin pwd tries to be "helpful" and remember
	  # symlink names. Use -P option, and hope it's portable enough.
	  test -d /usr/lib${ABI} \
	    && (cd /usr/lib${ABI} && pwd -P | grep >/dev/null "/lib${ABI}"'$') \
	    && libdir='${exec_prefix}/'"lib${ABI}"
	fi
	;;
      # On freebsd, it seems 32-bit libraries are in lib32,
      # and 64-bit in lib. Don't know about "kfreebsd", does
      # it follow the Linux fhs conventions?
      *:freebsd*:32)
	libdir='${exec_prefix}/lib32'
	;;
      *:freebsd*:64)
	libdir='${exec_prefix}/lib'
	;;
      *:irix*:32)
	libdir='${exec_prefix}/lib32'
	;;
      *:irix*:64)
	libdir='${exec_prefix}/lib64'
	;;
      *)
        AC_MSG_WARN([Don't know where to install $ABI-bit libraries on this system.]); dnl '

    esac
    AC_MSG_NOTICE([Libraries to be installed in $libdir.])
  fi
fi

OPT_NETTLE_SOURCES=""

# Select assembler code
asm_path=
if test "x$enable_assembler" = xyes ; then
  case "$host_cpu" in
    [i?86* | k[5-8]* | pentium* | athlon])
      asm_path=x86
      ;;
    [x86_64 | amd64])
      if test "$ABI" = 64 ; then
	asm_path=x86_64
	if test "x$enable_fat" = xyes ; then
	  asm_path="x86_64/fat $asm_path"
	  OPT_NETTLE_SOURCES="fat-x86_64.c $OPT_NETTLE_SOURCES"
	elif test "x$enable_x86_aesni" = xyes ; then
	  asm_path="x86_64/aesni $asm_path"
	fi
      else
	asm_path=x86
      fi
      ;;
    *sparc*)
      if test "$ABI" = 64 ; then
	asm_path=sparc64
      else
	asm_path=sparc32
      fi
      ;;
    arm*)
      asm_path=arm
      if test "x$enable_fat" = xyes ; then
	asm_path="arm/fat $asm_path"
	OPT_NETTLE_SOURCES="fat-arm.c $OPT_NETTLE_SOURCES"
      else
	case "$host_cpu" in
	  armv6* | armv7*)
	    NETTLE_CHECK_ARM_NEON

	    asm_path="arm/v6 arm"
      
	    if test "x$enable_arm_neon" = xyes ; then
	      asm_path="arm/neon $asm_path"
	    fi
	    ;;
	esac
      fi
      ;;
    *)
      enable_assembler=no
      ;;
  esac
fi

# Files which replace a C source file (or otherwise don't correspond
# to a new object file).
asm_replace_list="aes-encrypt-internal.asm aes-decrypt-internal.asm \
		arcfour-crypt.asm camellia-crypt-internal.asm \
		md5-compress.asm memxor.asm memxor3.asm \
		poly1305-internal.asm \
		chacha-core-internal.asm \
		salsa20-crypt.asm salsa20-core-internal.asm \
		serpent-encrypt.asm serpent-decrypt.asm \
		sha1-compress.asm sha256-compress.asm sha512-compress.asm \
		sha3-permute.asm umac-nh.asm umac-nh-n.asm machine.m4"

# Assembler files which generate additional object files if they are used.
asm_nettle_optional_list="gcm-hash8.asm cpuid.asm \
  aes-encrypt-internal-2.asm aes-decrypt-internal-2.asm memxor-2.asm \
  salsa20-core-internal-2.asm sha1-compress-2.asm sha256-compress-2.asm \
  sha3-permute-2.asm sha512-compress-2.asm \
  umac-nh-n-2.asm umac-nh-2.asm"

asm_hogweed_optional_list=""
if test "x$enable_public_key" = "xyes" ; then
  asm_hogweed_optional_list="ecc-192-modp.asm ecc-224-modp.asm \
    ecc-25519-modp.asm ecc-256-redc.asm ecc-384-modp.asm ecc-521-modp.asm"
fi

OPT_NETTLE_OBJS=""
OPT_HOGWEED_OBJS=""

asm_file_list=""

if test "x$enable_assembler" = xyes ; then
  if test -n "$asm_path"; then
    AC_MSG_NOTICE([Looking for assembler files in $asm_path.])
    for tmp_f in $asm_replace_list ; do
      for asm_dir in $asm_path ; do
        if test -f "$srcdir/$asm_dir/$tmp_f"; then
	  asm_file_list="$asm_file_list $tmp_f"
          AC_CONFIG_LINKS($tmp_f:$asm_dir/$tmp_f)
	  break
        fi
      done
    done
    dnl Workaround for AC_CONFIG_LINKS, which complains if we use the
    dnl same destination argument $tmp_f multiple times.
    for tmp_n in $asm_nettle_optional_list ; do
      dnl Note extra pair of [] in sed expression
      tmp_b=`echo "$tmp_n" | sed 's/\.[[^.]]*$//'`
      for asm_dir in $asm_path ; do
	if test -f "$srcdir/$asm_dir/$tmp_n"; then
	  asm_file_list="$asm_file_list $tmp_n"
	  AC_CONFIG_LINKS($tmp_n:$asm_dir/$tmp_n)
	  while read tmp_func ; do
	    AC_DEFINE_UNQUOTED(HAVE_NATIVE_$tmp_func)
	    eval HAVE_NATIVE_$tmp_func=yes
	  done <<EOF
[`sed -n 's/^.*[^ 	]*PROLOGUE(_*\(nettle_\)*\([^)]*\)).*$/\2/p' < "$srcdir/$asm_dir/$tmp_n"`]
EOF
	  OPT_NETTLE_OBJS="$OPT_NETTLE_OBJS $tmp_b"'.$(OBJEXT)'
	  break
	fi
      done
    done	
    for tmp_h in $asm_hogweed_optional_list ; do
      dnl Note extra pair of [] in sed expression
      tmp_b=`echo "$tmp_h" | sed 's/\.[[^.]]*$//'`
      for asm_dir in $asm_path ; do
	if test -f "$srcdir/$asm_dir/$tmp_h"; then
	  dnl Note double square brackets, for extra m4 quoting.
	  tmp_bits=`grep GMP_NUMB_BITS "$srcdir/$asm_dir/$tmp_h" \
            | sed 's/^.*GMP_NUMB_BITS(\([[0-9]]*\)).*$/\1/'`
	  if test "$tmp_bits" && test "$tmp_bits" != "${GMP_NUMB_BITS}" ; then
	     AC_MSG_WARN([skipping $tmp_h, because GMP_NUMB_BITS != $tmp_bits])
	     continue
	  fi
	  asm_file_list="$asm_file_list $tmp_h"
	  AC_CONFIG_LINKS($tmp_h:$asm_dir/$tmp_h)
	  while read tmp_func ; do
	    AC_DEFINE_UNQUOTED(HAVE_NATIVE_$tmp_func)
	    eval HAVE_NATIVE_$tmp_func=yes
	  done <<EOF
[`sed -n 's/[^ 	]*PROLOGUE(_*\(nettle_\)*\([^)]*\)).*$/\2/p' < "$srcdir/$asm_dir/$tmp_h"`]
EOF
	  OPT_HOGWEED_OBJS="$OPT_HOGWEED_OBJS $tmp_b"'.$(OBJEXT)'
	  break
	fi
      done
    done	
    if test -z "$asm_file_list"; then
      enable_assembler=no
      AC_MSG_WARN([No assembler files found.])
    fi
  fi
  case "$host_os" in
    darwin*)
      ASM_RODATA='.section __TEXT,__const'
      ;;
    *)
      ASM_RODATA='.section .rodata'
      ;;
  esac
fi

AC_SUBST([OPT_NETTLE_OBJS])
AC_SUBST([OPT_HOGWEED_OBJS])
AC_SUBST([OPT_NETTLE_SOURCES])
AC_SUBST([ASM_RODATA])
if test "x$enable_assembler" = xyes ; then
  IF_ASM=''
else
  IF_ASM='#'
fi
AC_SUBST([IF_ASM])

AH_VERBATIM([HAVE_NATIVE],
[/* Define to 1 each of the following for which a native (ie. CPU specific)
    implementation of the corresponding routine exists.  */
#undef HAVE_NATIVE_ecc_192_modp
#undef HAVE_NATIVE_ecc_192_redc
#undef HAVE_NATIVE_ecc_224_modp
#undef HAVE_NATIVE_ecc_224_redc
#undef HAVE_NATIVE_ecc_25519_modp
#undef HAVE_NATIVE_ecc_256_modp
#undef HAVE_NATIVE_ecc_256_redc
#undef HAVE_NATIVE_ecc_384_modp
#undef HAVE_NATIVE_ecc_384_redc
#undef HAVE_NATIVE_ecc_521_modp
#undef HAVE_NATIVE_ecc_521_redc
#undef HAVE_NATIVE_gcm_hash8
#undef HAVE_NATIVE_salsa20_core
#undef HAVE_NATIVE_sha1_compress
#undef HAVE_NATIVE_sha256_compress
#undef HAVE_NATIVE_sha512_compress
#undef HAVE_NATIVE_sha3_permute
#undef HAVE_NATIVE_umac_nh
#undef HAVE_NATIVE_umac_nh_n])

if test "x$enable_pic" = xyes; then
    LSH_CCPIC
else
    CCPIC=''
fi
AC_SUBST(CCPIC)

IF_DLL='#'
LIBNETTLE_FILE_SRC='$(LIBNETTLE_FORLINK)'
LIBHOGWEED_FILE_SRC='$(LIBHOGWEED_FORLINK)'
EMULATOR=''
W64_ABI=no

case "$host_os" in
  mingw32*|cygwin*)
    # The actual DLLs, e.g. libnettle-$major-$minor.dll, are normally
    # installed into the bin dir (or more exactly $libdir/../bin, for
    # automake), while libnettle.dll.a, which is a stub file for
    # linking to the DLL, is installed into the lib dir.
    case "$host_os" in
      mingw32*)
        LIBNETTLE_FORLINK='libnettle-$(LIBNETTLE_MAJOR)-$(LIBNETTLE_MINOR).dll'
        LIBHOGWEED_FORLINK='libhogweed-$(LIBHOGWEED_MAJOR)-$(LIBHOGWEED_MINOR).dll'
        ;;
      cygwin*)
        LIBNETTLE_FORLINK='cygnettle-$(LIBNETTLE_MAJOR)-$(LIBNETTLE_MINOR).dll'
        LIBHOGWEED_FORLINK='cyghogweed-$(LIBHOGWEED_MAJOR)-$(LIBHOGWEED_MINOR).dll'
        ;;
    esac
    if test "x$cross_compiling" = xyes ; then
	case "$ABI" in
	    64)
		EMULATOR=wine64
		;;
	    *)
		EMULATOR=wine
		;;
	esac
    fi
    if test "x$ABI" = x64 ; then
        W64_ABI=yes
    fi
    LIBNETTLE_SONAME=''
    LIBNETTLE_FILE='libnettle.dll.a'
    LIBNETTLE_FILE_SRC='$(LIBNETTLE_FILE)'
    LIBNETTLE_LINK='$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,--out-implib=$(LIBNETTLE_FILE) -Wl,--export-all-symbols -Wl,--enable-auto-import -Wl,--whole-archive'
    LIBNETTLE_LIBS='-Wl,--no-whole-archive $(LIBS)'

    LIBHOGWEED_SONAME=''
    LIBHOGWEED_FILE='libhogweed.dll.a'
    LIBHOGWEED_FILE_SRC='$(LIBHOGWEED_FILE)'
    LIBHOGWEED_LINK='$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,--out-implib=$(LIBHOGWEED_FILE) -Wl,--export-all-symbols -Wl,--enable-auto-import -Wl,--whole-archive'
    LIBHOGWEED_LIBS='-Wl,--no-whole-archive $(LIBS) libnettle.dll.a'
    IF_DLL=''
    ;;
  darwin*)
    LIBNETTLE_FORLINK=libnettle.dylib
    LIBNETTLE_SONAME='libnettle.$(LIBNETTLE_MAJOR).dylib'
    LIBNETTLE_FILE='libnettle.$(LIBNETTLE_MAJOR).$(LIBNETTLE_MINOR).dylib'
    LIBNETTLE_LINK='$(CC) $(CFLAGS) -dynamiclib $(LDFLAGS) -install_name ${libdir}/$(LIBNETTLE_SONAME) -compatibility_version $(LIBNETTLE_MAJOR) -current_version $(LIBNETTLE_MAJOR).$(LIBNETTLE_MINOR)'
    LIBNETTLE_LIBS=''

    LIBHOGWEED_FORLINK=libhogweed.dylib
    LIBHOGWEED_SONAME='libhogweed.$(LIBHOGWEED_MAJOR).dylib'
    LIBHOGWEED_FILE='libhogweed.$(LIBHOGWEED_MAJOR).$(LIBHOGWEED_MINOR).dylib'
    LIBHOGWEED_LINK='$(CC) $(CFLAGS) -dynamiclib -L. $(LDFLAGS) -install_name ${libdir}/$(LIBHOGWEED_SONAME) -compatibility_version $(LIBHOGWEED_MAJOR) -current_version $(LIBHOGWEED_MAJOR).$(LIBHOGWEED_MINOR)'
    LIBHOGWEED_LIBS='-lnettle $(LIBS)'
    ;;
  solaris*)
    # Sun's ld uses -h to set the soname, and this option is passed
    # through by both Sun's compiler and gcc. Might not work with GNU
    # ld, but it's unusual to use GNU ld on Solaris.
    LIBNETTLE_FORLINK=libnettle.so
    LIBNETTLE_SONAME='$(LIBNETTLE_FORLINK).$(LIBNETTLE_MAJOR)'
    LIBNETTLE_FILE='$(LIBNETTLE_SONAME).$(LIBNETTLE_MINOR)'
    LIBNETTLE_LINK='$(CC) $(CFLAGS) $(LDFLAGS) -G -h $(LIBNETTLE_SONAME)'
    LIBNETTLE_LIBS=''

    LIBHOGWEED_FORLINK=libhogweed.so
    LIBHOGWEED_SONAME='$(LIBHOGWEED_FORLINK).$(LIBHOGWEED_MAJOR)'
    LIBHOGWEED_FILE='$(LIBHOGWEED_SONAME).$(LIBHOGWEED_MINOR)'
    LIBHOGWEED_LINK='$(CC) $(CFLAGS) $(LDFLAGS) -G -h $(LIBHOGWEED_SONAME)'
    LIBHOGWEED_LIBS='libnettle.so $(LIBS)'
    ;;
  *)
    LIBNETTLE_FORLINK=libnettle.so
    LIBNETTLE_SONAME='$(LIBNETTLE_FORLINK).$(LIBNETTLE_MAJOR)'
    LIBNETTLE_FILE='$(LIBNETTLE_SONAME).$(LIBNETTLE_MINOR)'
    LIBNETTLE_LINK='$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname=$(LIBNETTLE_SONAME)'
    LIBNETTLE_LIBS=''

    LIBHOGWEED_FORLINK=libhogweed.so
    LIBHOGWEED_SONAME='$(LIBHOGWEED_FORLINK).$(LIBHOGWEED_MAJOR)'
    LIBHOGWEED_FILE='$(LIBHOGWEED_SONAME).$(LIBHOGWEED_MINOR)'
    LIBHOGWEED_LINK='$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname=$(LIBHOGWEED_SONAME)'
    # Requested by debian, to make linking with only -lhogweed work
    # (does not work in general, e.g., with static linking all of
    # -lhogweed -lgmp -lnettle are still required). Also makes dlopen
    # of libhogweed.so work, without having to use RTLD_GLOBAL.
    LIBHOGWEED_LIBS='libnettle.so $(LIBS)'
    ;;
esac

ASM_SYMBOL_PREFIX=''
ASM_ELF_STYLE='no'
ASM_COFF_STYLE='no'
# GNU as default is to use @
ASM_TYPE_FUNCTION='@function'
ASM_TYPE_PROGBITS='@progbits'
ASM_MARK_NOEXEC_STACK=''
ASM_ALIGN_LOG=''

if test x$enable_assembler = xyes ; then
  AC_CACHE_CHECK([if globals are prefixed by underscore],
  nettle_cv_asm_underscore,
  [ # Default is no underscore
    nettle_cv_asm_underscore=no
    AC_COMPILE_IFELSE(
      [AC_LANG_SOURCE([int a_global_symbol;])],
      [ $NM conftest.$OBJEXT >conftest.out
        if grep _a_global_symbol conftest.out >/dev/null ; then
          nettle_cv_asm_underscore=yes
        elif grep a_global_symbol conftest.out >/dev/null ; then
          nettle_cv_asm_underscore=no
        else
          AC_MSG_WARN([nm does not list a_global_symbol at all])
	fi],
      [AC_MSG_WARN([test program with a single global could not be compiled!?])])])
  if test x$nettle_cv_asm_underscore = xyes ; then
    ASM_SYMBOL_PREFIX='_'
  fi

  AC_CACHE_CHECK([for ELF-style .type,%function pseudo-ops],
    [nettle_cv_asm_type_percent_function],
    [GMP_TRY_ASSEMBLE([
.text
.globl foo
.type foo,%function
foo:
.Lend:

.size foo, .Lend - foo
],
       [nettle_cv_asm_type_percent_function=yes],
       [nettle_cv_asm_type_percent_function=no])])

dnl Needs double quote for the # character
  AC_CACHE_CHECK([[for ELF-style .type,#function pseudo-ops]],
    [nettle_cv_asm_type_hash_function],
    [GMP_TRY_ASSEMBLE([
.text
.globl foo
.type foo,#function
foo:
.Lend:

.size foo, .Lend - foo
],
       [nettle_cv_asm_type_hash_function=yes],
       [nettle_cv_asm_type_hash_function=no])])

  if test x$nettle_cv_asm_type_percent_function = xyes ; then
    ASM_ELF_STYLE='yes'
    ASM_TYPE_FUNCTION='%function'
    ASM_TYPE_PROGBITS='%progbits'
  else
    if test x$nettle_cv_asm_type_hash_function = xyes ; then
      ASM_ELF_STYLE='yes'
      ASM_TYPE_FUNCTION='#function'
      ASM_TYPE_PROGBITS='#progbits'
    fi
  fi

  AC_CACHE_CHECK([for COFF-style .type directive],
    [nettle_cv_asm_coff_type],
      [GMP_TRY_ASSEMBLE([
.text
.globl _foo
.def _foo
.scl 2
.type 32
.endef
_foo:
],
        [nettle_cv_asm_coff_type=yes],
        [nettle_cv_asm_coff_type=no])])
  if test "x$nettle_cv_asm_coff_type" = "xyes" ; then
    ASM_COFF_STYLE=yes
  fi

  AC_CACHE_CHECK([if we should use a .note.GNU-stack section],
  nettle_cv_asm_gnu_stack,
  [ # Default
    nettle_cv_asm_gnu_stack=no

    cat >conftest.c <<EOF
int foo() { return 0; }
EOF
    nettle_compile="$CC $CFLAGS $CPPFLAGS -c conftest.c >conftest.out 2>&1"
    if AC_TRY_EVAL(nettle_compile); then
      cat conftest.out >&AC_FD_CC
      $OBJDUMP -x conftest.o | grep '\.note\.GNU-stack' > /dev/null \
      && nettle_cv_asm_gnu_stack=yes
    else
      cat conftest.out >&AC_FD_CC
      echo "configure: failed program was:" >&AC_FD_CC
      cat conftest.s >&AC_FD_CC
    fi
    rm -f conftest.*])
  if test x$nettle_cv_asm_gnu_stack = xyes ; then
    ASM_MARK_NOEXEC_STACK='.section .note.GNU-stack,"",TYPE_PROGBITS'
  fi

  AC_CACHE_CHECK([if .align assembly directive is logarithmic],
    [nettle_cv_asm_align_log],
    [GMP_TRY_ASSEMBLE([
.align 3
],
       [nettle_cv_asm_align_log=yes],
       [nettle_cv_asm_align_log=no])])
  ASM_ALIGN_LOG="$nettle_cv_asm_align_log"
fi

AC_SUBST(ASM_SYMBOL_PREFIX)
AC_SUBST(ASM_ELF_STYLE)
AC_SUBST(ASM_COFF_STYLE)
AC_SUBST(ASM_TYPE_FUNCTION)
AC_SUBST(ASM_TYPE_PROGBITS)
AC_SUBST(ASM_MARK_NOEXEC_STACK)
AC_SUBST(ASM_ALIGN_LOG)
AC_SUBST(W64_ABI)
AC_SUBST(EMULATOR)

AC_SUBST(LIBNETTLE_MAJOR)
AC_SUBST(LIBNETTLE_MINOR)
AC_SUBST(LIBNETTLE_FORLINK)
AC_SUBST(LIBNETTLE_SONAME)
AC_SUBST(LIBNETTLE_FILE)
AC_SUBST(LIBNETTLE_FILE_SRC)
AC_SUBST(LIBNETTLE_LINK)
AC_SUBST(LIBNETTLE_LIBS)

AC_SUBST(LIBHOGWEED_MAJOR)
AC_SUBST(LIBHOGWEED_MINOR)
AC_SUBST(LIBHOGWEED_FORLINK)
AC_SUBST(LIBHOGWEED_SONAME)
AC_SUBST(LIBHOGWEED_FILE)
AC_SUBST(LIBHOGWEED_FILE_SRC)
AC_SUBST(LIBHOGWEED_LINK)
AC_SUBST(LIBHOGWEED_LIBS)

AC_PATH_PROG(M4, m4, m4)

AH_TEMPLATE([WITH_HOGWEED], [Defined if public key features are enabled])

if test "x$enable_public_key" = xyes ; then
  AC_DEFINE(WITH_HOGWEED)
  IF_HOGWEED=''
else
  IF_HOGWEED='#'
fi

if test "x$enable_static" = xyes ; then
  IF_STATIC=''
else
  IF_STATIC='#'
fi

IF_DLOPEN_TEST='#'
if test "x$enable_shared" = xyes ; then
  IF_SHARED=''
  IF_NOT_SHARED='#'
  if test "x$ac_cv_lib_dl_dlopen" = xyes ; then
    IF_DLOPEN_TEST=''
  fi
else
  IF_SHARED='#'
  IF_NOT_SHARED=''
fi

# Documentation tools
if test "x$enable_documentation" != "xno"; then
  AC_PATH_PROG(MAKEINFO, makeinfo, not-found)

  if test "x$MAKEINFO" != "xnot-found"; then
    enable_documentation=yes
    AC_SUBST(MAKEINFO)
  else
    if test "x$enable_documentation" == "xauto" ; then
      enable_documentation=no
    else
      AC_MSG_ERROR([Cannot find 'makeinfo', required for documentation.])
    fi
  fi
fi

if test "x$enable_documentation" = "xyes" ; then
  IF_DOCUMENTATION=''
else
  IF_DOCUMENTATION='#'
fi

if test "x$enable_mini_gmp" = "xyes" ; then
  IF_MINI_GMP=''
else
  IF_MINI_GMP='#'
fi
  
AC_SUBST(IF_HOGWEED)
AC_SUBST(IF_STATIC)
AC_SUBST(IF_SHARED)
AC_SUBST(IF_NOT_SHARED)
AC_SUBST(IF_DLOPEN_TEST)
AC_SUBST(IF_DOCUMENTATION)
AC_SUBST(IF_DLL)
AC_SUBST(IF_MINI_GMP)

OPENSSL_LIBFLAGS=''

# Check for openssl's libcrypto (used only for benchmarking)
if test x$enable_openssl = xyes ; then
  AC_CHECK_LIB(crypto, BF_ecb_encrypt,
    [OPENSSL_LIBFLAGS='-lcrypto'],
    [enable_openssl=no])
fi

AH_TEMPLATE([WITH_OPENSSL],
	    [Define if you have openssl's libcrypto (used for benchmarking)]) dnl'

if test x$enable_openssl = xyes ; then
  AC_DEFINE(WITH_OPENSSL)
fi

AC_SUBST(OPENSSL_LIBFLAGS)

AH_BOTTOM(
[#if defined(__x86_64__) || defined(__arch64__)
# define HAVE_NATIVE_64_BIT 1
#else
/* Needs include of <limits.h> before use. */
# define HAVE_NATIVE_64_BIT (SIZEOF_LONG * CHAR_BIT >= 64)
#endif
])

# clock_gettime is in librt on *-*-osf5.1 and on glibc, so add -lrt to
# BENCH_LIBS if needed. On linux (tested on x86_32, 2.6.26),
# clock_getres reports ns accuracy, while in a quick test on osf
# clock_getres said only 1 millisecond.

old_LIBS="$LIBS"
AC_SEARCH_LIBS(clock_gettime, rt, [
  AC_DEFINE([HAVE_CLOCK_GETTIME],1,[Define if clock_gettime is available])])
BENCH_LIBS="$LIBS"
LIBS="$old_LIBS"

AC_SUBST(BENCH_LIBS)

# Set these flags *last*, or else the test programs won't compile
if test x$GCC = xyes ; then
  # Using -ggdb3 makes (some versions of) Redhat's gcc-2.96 dump core
  if $CC --version | grep '^2\.96$' 1>/dev/null 2>&1; then
    true
  else
    CFLAGS="$CFLAGS -ggdb3"
  fi
  # FIXME: It would be better to actually test if this option works and/or is needed.
  # Or perhaps use -funsigned-char.
  if $CC --version | grep 'gcc.* 4\.' 1>/dev/null 2>&1; then
    CFLAGS="$CFLAGS -Wno-pointer-sign"
  fi
  CFLAGS="$CFLAGS -Wall -W \
  -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes \
  -Wpointer-arith -Wbad-function-cast -Wnested-externs"

# Don't enable -Wcast-align as it results in tons of warnings in the
# DES code. And when using stdio.
# Don't enable -Waggregate-return, as that causes warnings for glibc
# inttypes.h.
fi

AC_CONFIG_FILES([config.make config.m4 Makefile version.h])
AC_CONFIG_FILES([tools/Makefile testsuite/Makefile examples/Makefile])
AC_CONFIG_FILES([nettle.pc hogweed.pc libnettle.map libhogweed.map])

AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

  Version:           ${PACKAGE_STRING}
  Host type:         ${host}
  ABI:               ${ABI}
  Assembly files:    ${asm_path:-none}
  Install prefix:    ${prefix}
  Library directory: ${libdir}
  Compiler:          ${CC}
  Static libraries:  ${enable_static}
  Shared libraries:  ${enable_shared}
  Public key crypto: ${enable_public_key}
  Using mini-gmp:    ${enable_mini_gmp}
  Documentation:     ${enable_documentation}
])
