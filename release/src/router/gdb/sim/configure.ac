dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)

AC_PROG_CC
AC_PROG_INSTALL
AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(RANLIB, ranlib, :)

AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM
AC_PROG_CC
AC_SUBST(CFLAGS)
AC_SUBST(HDEFINES)
AR=${AR-ar}
AC_SUBST(AR)
AC_PROG_RANLIB

# Put a plausible default for CC_FOR_BUILD in Makefile.
if test "x$cross_compiling" = "xno"; then
  CC_FOR_BUILD='$(CC)'
else
  CC_FOR_BUILD=gcc
fi
AC_SUBST(CC_FOR_BUILD)
CFLAGS_FOR_BUILD=${CFLAGS_FOR_BUILD-${CFLAGS}}
AC_SUBST(CFLAGS_FOR_BUILD)

# If a cpu ever has more than one simulator to choose from, use
# --enable-sim=... to choose.
AC_ARG_ENABLE(sim,
[  --enable-sim ],
[case "${enableval}" in
yes | no) ;;
*)	AC_MSG_ERROR(bad value ${enableval} given for --enable-sim option) ;;
esac])

# WHEN ADDING ENTRIES TO THIS MATRIX:

# Make sure that the left side always has two dashes.  Otherwise you
# can get spurious matches.  Even for unambiguous cases, do this as a
# convention, else the table becomes a real mess to understand and
# maintain.

if test "${enable_sim}" != no; then
   testsuite=no
   common=yes
   igen=no
   case "${target}" in
       arm*-*-* | thumb*-*-* | strongarm*-*-* | xscale-*-*)
           AC_CONFIG_SUBDIRS(arm)
	   testsuite=yes
	   ;;
       cris-*-* | crisv32-*-*)
	   AC_CONFIG_SUBDIRS(cris)
	   testsuite=yes
	   ;;
       d10v-*-*)
           AC_CONFIG_SUBDIRS(d10v)
	   ;;
       frv-*-*)
           AC_CONFIG_SUBDIRS(frv)
	   testsuite=yes
	   ;;
       h8300*-*-*)
           AC_CONFIG_SUBDIRS(h8300)
	   testsuite=yes
	   ;;
       iq2000-*-*)
           AC_CONFIG_SUBDIRS(iq2000)
	   testsuite=yes
	   ;;
       m32c-*-*)
           AC_CONFIG_SUBDIRS(m32c)
           ;;
       m32r-*-*)
           AC_CONFIG_SUBDIRS(m32r)
	   testsuite=yes
	   ;;
       m68hc11-*-*|m6811-*-*)
           AC_CONFIG_SUBDIRS(m68hc11)
	   testsuite=yes
	   ;;
       mcore-*-*)
           AC_CONFIG_SUBDIRS(mcore)
	   testsuite=yes
	   ;;
       mips*-*-*)
           AC_CONFIG_SUBDIRS(mips)
	   testsuite=yes
	   igen=yes
	   ;;
       mn10300*-*-*)
           AC_CONFIG_SUBDIRS(mn10300)
	   igen=yes
	   ;;
       sh64*-*-*)
           AC_CONFIG_SUBDIRS(sh64)
	   testsuite=yes
	   ;;
       sh*-*-*)
           AC_CONFIG_SUBDIRS(sh)
	   testsuite=yes
	   ;;
       sparc-*-rtems*|sparc-*-elf*)
	   AC_CONFIG_SUBDIRS(erc32)
	   testsuite=yes
	   ;;
       powerpc*-*-* )
           AC_CONFIG_SUBDIRS(ppc)
	   ;;
       v850*-*-* )
           AC_CONFIG_SUBDIRS(v850)
	   igen=yes
	   ;;
       *)
	   # No simulator subdir, so the subdir "common" isn't needed.
	   common=no
	   ;;
   esac
   if test "$testsuite" = yes; then
      AC_CONFIG_SUBDIRS(testsuite)
   fi
   if test "$common" = yes; then
      AC_CONFIG_SUBDIRS(common)
   fi
   if test "$igen" = yes; then
      AC_CONFIG_SUBDIRS(igen)
   fi
fi

AC_OUTPUT(Makefile)

exit 0
