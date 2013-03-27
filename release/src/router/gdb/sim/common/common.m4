# This file contains common code used by all simulators.
#
# common.m4 invokes AC macros used by all simulators and by the common
# directory.  It is intended to be included before any target specific
# stuff.  SIM_AC_OUTPUT is a cover function to AC_OUTPUT to generate
# the Makefile.  It is intended to be invoked last.
#
# The simulator's configure.in should look like:
#
# dnl Process this file with autoconf to produce a configure script.
# AC_PREREQ(2.5)dnl
# AC_INIT(Makefile.in)
# AC_CONFIG_HEADER(config.h:config.in)
#
# sinclude(../common/aclocal.m4)
# sinclude(../common/common.m4)
#
# ... target specific stuff ...

AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM
AC_PROG_CC
AC_PROG_INSTALL

# Put a plausible default for CC_FOR_BUILD in Makefile.
if test "x$cross_compiling" = "xno"; then
  CC_FOR_BUILD='$(CC)'
else
  CC_FOR_BUILD=gcc
fi
AC_SUBST(CC_FOR_BUILD)

AC_SUBST(CFLAGS)
AC_SUBST(HDEFINES)
AR=${AR-ar}
AC_SUBST(AR)
AC_PROG_RANLIB

dnl We don't use gettext, but bfd does.  So we do the appropriate checks
dnl to see if there are intl libraries we should link against.
ALL_LINGUAS=
ZW_GNU_GETTEXT_SISTER_DIR(../../intl)

# Check for common headers.
# FIXME: Seems to me this can cause problems for i386-windows hosts.
# At one point there were hardcoded AC_DEFINE's if ${host} = i386-*-windows*.
AC_CHECK_HEADERS(stdlib.h string.h strings.h unistd.h time.h)
AC_CHECK_HEADERS(sys/time.h sys/resource.h)
AC_CHECK_HEADERS(fcntl.h fpu_control.h)
AC_CHECK_HEADERS(dlfcn.h errno.h sys/stat.h)
AC_CHECK_FUNCS(getrusage time sigaction __setfpucw)

# Check for socket libraries
AC_CHECK_LIB(socket, bind)
AC_CHECK_LIB(nsl, gethostbyname)

. ${srcdir}/../../bfd/configure.host

dnl Standard (and optional) simulator options.
dnl Eventually all simulators will support these.
dnl Do not add any here that cannot be supported by all simulators.
dnl Do not add similar but different options to a particular simulator,
dnl all shall eventually behave the same way.


dnl We don't use automake, but we still want to support
dnl --enable-maintainer-mode.
USE_MAINTAINER_MODE=no
AC_ARG_ENABLE(maintainer-mode,
[  --enable-maintainer-mode		Enable developer functionality.],
[case "${enableval}" in
  yes)	MAINT="" USE_MAINTAINER_MODE=yes ;;
  no)	MAINT="#" ;;
  *)	AC_MSG_ERROR("--enable-maintainer-mode does not take a value"); MAINT="#" ;;
esac
if test x"$silent" != x"yes" && test x"$MAINT" = x""; then
  echo "Setting maintainer mode" 6>&1
fi],[MAINT="#"])dnl
AC_SUBST(MAINT)


dnl This is a generic option to enable special byte swapping
dnl insns on *any* cpu.
AC_ARG_ENABLE(sim-bswap,
[  --enable-sim-bswap			Use Host specific BSWAP instruction.],
[case "${enableval}" in
  yes)	sim_bswap="-DWITH_BSWAP=1 -DUSE_BSWAP=1";;
  no)	sim_bswap="-DWITH_BSWAP=0";;
  *)	AC_MSG_ERROR("--enable-sim-bswap does not take a value"); sim_bswap="";;
esac
if test x"$silent" != x"yes" && test x"$sim_bswap" != x""; then
  echo "Setting bswap flags = $sim_bswap" 6>&1
fi],[sim_bswap=""])dnl
AC_SUBST(sim_bswap)


AC_ARG_ENABLE(sim-cflags,
[  --enable-sim-cflags=opts		Extra CFLAGS for use in building simulator],
[case "${enableval}" in
  yes)	 sim_cflags="-O2 -fomit-frame-pointer";;
  trace) AC_MSG_ERROR("Please use --enable-sim-debug instead."); sim_cflags="";;
  no)	 sim_cflags="";;
  *)	 sim_cflags=`echo "${enableval}" | sed -e "s/,/ /g"`;;
esac
if test x"$silent" != x"yes" && test x"$sim_cflags" != x""; then
  echo "Setting sim cflags = $sim_cflags" 6>&1
fi],[sim_cflags=""])dnl
AC_SUBST(sim_cflags)


dnl --enable-sim-debug is for developers of the simulator
dnl the allowable values are work-in-progress
AC_ARG_ENABLE(sim-debug,
[  --enable-sim-debug=opts		Enable debugging flags],
[case "${enableval}" in
  yes) sim_debug="-DDEBUG=7 -DWITH_DEBUG=7";;
  no)  sim_debug="-DDEBUG=0 -DWITH_DEBUG=0";;
  *)   sim_debug="-DDEBUG='(${enableval})' -DWITH_DEBUG='(${enableval})'";;
esac
if test x"$silent" != x"yes" && test x"$sim_debug" != x""; then
  echo "Setting sim debug = $sim_debug" 6>&1
fi],[sim_debug=""])dnl
AC_SUBST(sim_debug)


dnl --enable-sim-stdio is for users of the simulator
dnl It determines if IO from the program is routed through STDIO (buffered)
AC_ARG_ENABLE(sim-stdio,
[  --enable-sim-stdio			Specify whether to use stdio for console input/output.],
[case "${enableval}" in
  yes)	sim_stdio="-DWITH_STDIO=DO_USE_STDIO";;
  no)	sim_stdio="-DWITH_STDIO=DONT_USE_STDIO";;
  *)	AC_MSG_ERROR("Unknown value $enableval passed to --enable-sim-stdio"); sim_stdio="";;
esac
if test x"$silent" != x"yes" && test x"$sim_stdio" != x""; then
  echo "Setting stdio flags = $sim_stdio" 6>&1
fi],[sim_stdio=""])dnl
AC_SUBST(sim_stdio)


dnl --enable-sim-trace is for users of the simulator
dnl The argument is either a bitmask of things to enable [exactly what is
dnl up to the simulator], or is a comma separated list of names of tracing
dnl elements to enable.  The latter is only supported on simulators that
dnl use WITH_TRACE.
AC_ARG_ENABLE(sim-trace,
[  --enable-sim-trace=opts		Enable tracing flags],
[case "${enableval}" in
  yes)	sim_trace="-DTRACE=1 -DWITH_TRACE=-1";;
  no)	sim_trace="-DTRACE=0 -DWITH_TRACE=0";;
  [[-0-9]]*)
	sim_trace="-DTRACE='(${enableval})' -DWITH_TRACE='(${enableval})'";;
  [[a-z]]*)
	sim_trace=""
	for x in `echo "$enableval" | sed -e "s/,/ /g"`; do
	  if test x"$sim_trace" = x; then
	    sim_trace="-DWITH_TRACE='(TRACE_$x"
	  else
	    sim_trace="${sim_trace}|TRACE_$x"
	  fi
	done
	sim_trace="$sim_trace)'" ;;
esac
if test x"$silent" != x"yes" && test x"$sim_trace" != x""; then
  echo "Setting sim trace = $sim_trace" 6>&1
fi],[sim_trace=""])dnl
AC_SUBST(sim_trace)


dnl --enable-sim-profile
dnl The argument is either a bitmask of things to enable [exactly what is
dnl up to the simulator], or is a comma separated list of names of profiling
dnl elements to enable.  The latter is only supported on simulators that
dnl use WITH_PROFILE.
AC_ARG_ENABLE(sim-profile,
[  --enable-sim-profile=opts		Enable profiling flags],
[case "${enableval}" in
  yes)	sim_profile="-DPROFILE=1 -DWITH_PROFILE=-1";;
  no)	sim_profile="-DPROFILE=0 -DWITH_PROFILE=0";;
  [[-0-9]]*)
	sim_profile="-DPROFILE='(${enableval})' -DWITH_PROFILE='(${enableval})'";;
  [[a-z]]*)
	sim_profile=""
	for x in `echo "$enableval" | sed -e "s/,/ /g"`; do
	  if test x"$sim_profile" = x; then
	    sim_profile="-DWITH_PROFILE='(PROFILE_$x"
	  else
	    sim_profile="${sim_profile}|PROFILE_$x"
	  fi
	done
	sim_profile="$sim_profile)'" ;;
esac
if test x"$silent" != x"yes" && test x"$sim_profile" != x""; then
  echo "Setting sim profile = $sim_profile" 6>&1
fi],[sim_profile="-DPROFILE=1 -DWITH_PROFILE=-1"])dnl
AC_SUBST(sim_profile)


dnl Types used by common code
AC_TYPE_SIGNAL

dnl Detect exe extension
AC_EXEEXT

dnl These are available to append to as desired.
sim_link_files=
sim_link_links=

dnl Create tconfig.h either from simulator's tconfig.in or default one
dnl in common.
sim_link_links=tconfig.h
if test -f ${srcdir}/tconfig.in
then
  sim_link_files=tconfig.in
else
  sim_link_files=../common/tconfig.in
fi

# targ-vals.def points to the libc macro description file.
case "${target}" in
*-*-*) TARG_VALS_DEF=../common/nltvals.def ;;
esac
sim_link_files="${sim_link_files} ${TARG_VALS_DEF}"
sim_link_links="${sim_link_links} targ-vals.def"
