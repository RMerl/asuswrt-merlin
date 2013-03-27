dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)
AC_CONFIG_HEADER(config.h:config.in)

sinclude(../common/aclocal.m4)

# Bugs in autoconf 2.59 break the call to SIM_AC_COMMON, hack around
# it by inlining the macro's contents.
sinclude(../common/common.m4)

SIM_AC_OPTION_ENDIAN(BIG_ENDIAN)
SIM_AC_OPTION_ALIGNMENT(STRICT_ALIGNMENT)
SIM_AC_OPTION_HOSTENDIAN
SIM_AC_OPTION_SCACHE(16384)
SIM_AC_OPTION_DEFAULT_MODEL(fr500)
SIM_AC_OPTION_ENVIRONMENT
SIM_AC_OPTION_CGEN_MAINT

#
# Enable making unknown traps dump out registers
#
AC_ARG_ENABLE(sim-trapdump,
[  --enable-sim-trapdump		Make unknown traps dump the registers],
[case "${enableval}" in
  yes) sim_trapdump="-DTRAPDUMP=1";;
  no)  sim_trapdump="-DTRAPDUMP=0";;
  *)   AC_MSG_ERROR("Unknown value $enableval passed to --enable-sim-trapdump"); sim_trapdump="";;
esac
if test x"$silent" != x"yes" && test x"$sim_trapdump" != x""; then
  echo "Setting sim_trapdump = $sim_trapdump" 6>&1
fi],[sim_trapdump=""])dnl
AC_SUBST(sim_trapdump)

SIM_AC_OUTPUT
