dnl Process this file with autoconf to produce a configure script.
sinclude(../common/aclocal.m4)
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)

AC_PROG_INSTALL
AC_PROG_CC

# Put a plausible default for CC_FOR_BUILD in Makefile.
if test "x$cross_compiling" = "xno"; then
  CC_FOR_BUILD='$(CC)'
else
  CC_FOR_BUILD=gcc
fi
CFLAGS_FOR_BUILD=${CFLAGS_FOR_BUILD-${CFLAGS}}

dnl We don't use gettext, but bfd does.  So we do the appropriate checks
dnl to see if there are intl libraries we should link against.
ALL_LINGUAS=
ZW_GNU_GETTEXT_SISTER_DIR


AC_ARG_ENABLE(sim-alignment,
[  --enable-sim-alignment=align		Specify strict or nonstrict alignment.],
[case "${enableval}" in
  yes | strict | STRICT)	sim_alignment="-DWITH_ALIGNMENT=STRICT_ALIGNMENT";;
  no | nonstrict | NONSTRICT)	sim_alignment="-DWITH_ALIGNMENT=NONSTRICT_ALIGNMENT";;
  0 | default | DEFAULT)	sim_alignment="-DWITH_ALIGNMENT=0";;
  *)				AC_MSG_ERROR("Unknown value $enableval passed to --enable-sim-alignment"); sim_alignment="";;
esac
if test x"$silent" != x"yes" && test x"$sim_alignment" != x""; then
  echo "Setting alignment flags = $sim_alignment" 6>&1
fi],[sim_alignment=""])dnl


AC_ARG_ENABLE(sim-assert,
[  --enable-sim-assert			Specify whether to perform random assertions.],
[case "${enableval}" in
  yes)	sim_assert="-DWITH_ASSERT=1";;
  no)	sim_assert="-DWITH_ASSERT=0";;
  *)	AC_MSG_ERROR("--enable-sim-assert does not take a value"); sim_assert="";;
esac
if test x"$silent" != x"yes" && test x"$sim_assert" != x""; then
  echo "Setting assert flags = $sim_assert" 6>&1
fi],[sim_assert=""])dnl


AC_ARG_ENABLE(sim-bitsize,
[  --enable-sim-bitsize=n		Specify target bitsize (32 or 64).],
[case "${enableval}" in
  32|64) sim_bitsize="-DWITH_TARGET_WORD_BITSIZE=$enableval";;
  *)	 AC_MSG_ERROR("--enable-sim-bitsize was given $enableval.  Expected 32 or 64"); sim_bitsize="";;
esac
if test x"$silent" != x"yes" && test x"$sim_bitsize" != x""; then
  echo "Setting bitsize flags = $sim_bitsize" 6>&1
fi],[sim_bitsize=""])dnl


AC_ARG_ENABLE(sim-bswap,
[  --enable-sim-bswap			Use the BSWAP instruction on Intel 486s and Pentiums.],
[case "${enableval}" in
  yes)	sim_bswap="-DWITH_BSWAP=1";;
  no)	sim_bswap="-DWITH_BSWAP=0";;
  *)	AC_MSG_ERROR("--enable-sim-bswap does not take a value"); sim_bswap="";;
esac
if test x"$silent" != x"yes" && test x"$sim_bswap" != x""; then
  echo "Setting bswap flags = $sim_bswap" 6>&1
fi],[sim_bswap=""])dnl


AC_ARG_ENABLE(sim-cflags,
[  --enable-sim-cflags=opts		Extra CFLAGS for use in building simulator],
[case "${enableval}" in
  yes)	sim_cflags="-O2 -fomit-frame-pointer";;
  no)	sim_cflags="";;
  *)	sim_cflags=`echo "${enableval}" | sed -e "s/,/ /g"`;;
esac
if test x"$silent" != x"yes" && test x"$sim_cflags" != x""; then
  echo "Setting sim cflags = $sim_cflags" 6>&1
fi],[sim_cflags=""])dnl


AC_ARG_ENABLE(sim-config,
[  --enable-sim-config=file		Override default config file],
[case "${enableval}" in
  yes|no)	AC_MSG_ERROR("No value supplied for --enable-sim-config=file");;
  *)		if test -f "${srcdir}/${enableval}"; then
		  sim_config="${enableval}";
		elif test -f "${srcdir}/${enableval}-config.h"; then
		  sim_config="${enableval}-config.h"
		else
		  AC_MSG_ERROR("Config file $enableval was not found");
		  sim_config=std-config.h
		fi;;
esac
if test x"$silent" != x"yes" && test x"$sim_config" != x""; then
  echo "Setting config flags = $sim_config" 6>&1
fi],[sim_config="std-config.h"
if test x"$silent" != x"yes"; then
  echo "Setting config flags = $sim_config" 6>&1
fi])dnl


AC_ARG_ENABLE(sim-decode-mechanism,
[  --enable-sim-decode-mechanism=which		Specify the instruction decode mechanism.],
[case "${enableval}" in
  yes|no)	AC_MSG_ERROR("No value supplied for --enable-sim-decode-mechanism=file");;
  array|switch|padded-switch|goto-switch)	sim_decode_mechanism="-T ${enableval}";;
  *)		AC_MSG_ERROR("File $enableval is not an opcode rules file");
		sim_decode_mechanism="switch";;
esac
if test x"$silent" != x"yes" && test x"$sim_decode_mechanism" != x""; then
  echo "Setting decode mechanism flags = $sim_decode_mechanism" 6>&1
fi],[sim_decode_mechanism=""
if test x"$silent" != x"yes"; then
  echo "Setting decode mechanism flags = $sim_decode_mechanism"
fi])dnl


AC_ARG_ENABLE(sim-default-model,
[  --enable-sim-default-model=which	Specify default PowerPC to model.],
[case "${enableval}" in
  yes|no)	AC_MSG_ERROR("No value supplied for --enable-sim-default-model=model");;
  *)		sim_default_model="-DWITH_DEFAULT_MODEL=${enableval}";;
esac
if test x"$silent" != x"yes" && test x"$sim_default_model" != x""; then
  echo "Setting default-model flags = $sim_default_model" 6>&1
fi],[sim_default_model=""])dnl


AC_ARG_ENABLE(sim-duplicate,
[  --enable-sim-duplicate		Expand (duplicate) semantic functions.],
[case "${enableval}" in
  yes)	sim_dup="-E";;
  no)	sim_dup="";;
  *)	AC_MSG_ERROR("--enable-sim-duplicate does not take a value"); sim_dup="";;
esac
if test x"$silent" != x"yes" && test x"$sim_dup" != x""; then
  echo "Setting duplicate flags = $sim_dup" 6>&1
fi],[sim_dup="-E"
if test x"$silent" != x"yes"; then
  echo "Setting duplicate flags = $sim_dup" 6>&1
fi])dnl


AC_ARG_ENABLE(sim-endian,
[  --enable-sim-endian=endian		Specify target byte endian orientation.],
[case "${enableval}" in
  yes)	case "$target" in
	  *powerpc-*) sim_endian="-DWITH_TARGET_BYTE_ORDER=BIG_ENDIAN";;
	  *powerpcle-*) sim_endian="-DWITH_TARGET_BYTE_ORDER=LITTLE_ENDIAN";;
	  *) echo "Unknown target $target" 1>&6; sim_endian="-DWITH_TARGET_BYTE_ORDER=0";;
	esac;;
  no)	 sim_endian="-DWITH_TARGET_BYTE_ORDER=0";;
  b*|B*) sim_endian="-DWITH_TARGET_BYTE_ORDER=BIG_ENDIAN";;
  l*|L*) sim_endian="-DWITH_TARGET_BYTE_ORDER=LITTLE_ENDIAN";;
  *)	 AC_MSG_ERROR("Unknown value $enableval for --enable-sim-endian"); sim_endian="";;
esac
if test x"$silent" != x"yes" && test x"$sim_endian" != x""; then
  echo "Setting endian flags = $sim_endian" 6>&1
fi],[sim_endian=""])dnl


AC_ARG_ENABLE(sim-env,
[  --enable-sim-env=env			Specify target environment (operating, virtual, user).],
[case "${enableval}" in
  operating | os | oea)	sim_env="-DWITH_ENVIRONMENT=OPERATING_ENVIRONMENT";;
  virtual | vea)	sim_env="-DWITH_ENVIRONMENT=VIRTUAL_ENVIRONMENT";;
  user | uea)		sim_env="-DWITH_ENVIRONMENT=USER_ENVIRONMENT";;
  no)			sim_env="-DWITH_ENVIRONMENT=0";;
  *)			AC_MSG_ERROR("Unknown value $enableval passed to --enable-sim-env"); sim_env="";;
esac
if test x"$silent" != x"yes" && test x"$sim_env" != x""; then
  echo "Setting env flags = $sim_env" 6>&1
fi],[sim_env=""])dnl


AC_ARG_ENABLE(sim-filter,
[  --enable-sim-filter=rule		Specify filter rules.],
[case "${enableval}" in
  yes)	AC_MSG_ERROR("--enable-sim-filter must be specified with a rule to filter or no"); sim_filter="";;
  no)	sim_filter="";;
  *)	sim_filter="-F $enableval";;
esac
if test x"$silent" != x"yes" && test x"$sim_filter" != x""; then
  echo "Setting filter flags = $sim_filter" 6>&1
fi],[sim_filter="-F 32,f,o"
if test x"$silent" != x"yes"; then
  echo "Setting filter flags = $sim_filter" 6>&1
fi])dnl


AC_ARG_ENABLE(sim-float,
[  --enable-sim-float			Specify whether the target has hard, soft, altivec or e500 floating point.],
[case "${enableval}" in
  yes | hard)	sim_float="-DWITH_FLOATING_POINT=HARD_FLOATING_POINT";;
  no | soft)	sim_float="-DWITH_FLOATING_POINT=SOFT_FLOATING_POINT";;
  altivec)      sim_float="-DWITH_ALTIVEC" ; sim_filter="${sim_filter},av" ;;
  *spe*|*simd*) sim_float="-DWITH_E500" ; sim_filter="${sim_filter},e500" ;;
  *)		AC_MSG_ERROR("Unknown value $enableval passed to --enable-sim-float"); sim_float="";;
esac
if test x"$silent" != x"yes" && test x"$sim_float" != x""; then
  echo "Setting float flags = $sim_float" 6>&1
fi],[
case "${target}" in
  *altivec*) sim_float="-DWITH_ALTIVEC" ; sim_filter="${sim_filter},av" ;;
  *spe*|*simd*)	sim_float="-DWITH_E500" ; sim_filter="${sim_filter},e500" ;;
  *) sim_float=""
esac
])dnl


AC_ARG_ENABLE(sim-hardware,
[  --enable-sim-hardware=list		Specify the hardware to be included in the build.],
[hardware="cpu,memory,nvram,iobus,htab,disk,trace,register,vm,init,core,pal,com,eeprom,opic,glue,phb,ide"
case "${enableval}" in
  yes)	;;
  no)	AC_MSG_ERROR("List of hardware must be specified for --enable-sim-hardware"); hardware="";;
  ,*)   hardware="${hardware}${enableval}";;
  *,)   hardware="${enableval}${hardware}";;
  *)	hardware="${enableval}"'';;
esac
sim_hw_src=`echo $hardware | sed -e 's/,/.c hw_/g' -e 's/^/hw_/' -e s'/$/.c/'`
sim_hw_obj=`echo $sim_hw_src | sed -e 's/\.c/.o/g'`
if test x"$silent" != x"yes" && test x"$hardware" != x""; then
  echo "Setting hardware to $sim_hw_src, $sim_hw_obj"
fi],[hardware="cpu,memory,nvram,iobus,htab,disk,trace,register,vm,init,core,pal,com,eeprom,opic,glue,phb,ide"
sim_hw_src=`echo $hardware | sed -e 's/,/.c hw_/g' -e 's/^/hw_/' -e s'/$/.c/'`
sim_hw_obj=`echo $sim_hw_src | sed -e 's/\.c/.o/g'`
if test x"$silent" != x"yes"; then
  echo "Setting hardware to $sim_hw_src, $sim_hw_obj"
fi])dnl


AC_ARG_ENABLE(sim-hostbitsize,
[  --enable-sim-hostbitsize=32|64	Specify host bitsize (32 or 64).],
[case "${enableval}" in
  32|64) sim_hostbitsize="-DWITH_HOST_WORD_BITSIZE=$enableval";;
  *)	 AC_MSG_ERROR("--enable-sim-hostbitsize was given $enableval.  Expected 32 or 64"); sim_hostbitsize="";;
esac
if test x"$silent" != x"yes" && test x"$sim_hostbitsize" != x""; then
  echo "Setting hostbitsize flags = $sim_hostbitsize" 6>&1
fi],[sim_hostbitsize=""])dnl


AC_ARG_ENABLE(sim-hostendian,
[  --enable-sim-hostendian=end		Specify host byte endian orientation.],
[case "${enableval}" in
  no)	 sim_hostendian="-DWITH_HOST_BYTE_ORDER=0";;
  b*|B*) sim_hostendian="-DWITH_HOST_BYTE_ORDER=BIG_ENDIAN";;
  l*|L*) sim_hostendian="-DWITH_HOST_BYTE_ORDER=LITTLE_ENDIAN";;
  *)	 AC_MSG_ERROR("Unknown value $enableval for --enable-sim-hostendian"); sim_hostendian="";;
esac
if test x"$silent" != x"yes" && test x"$sim_hostendian" != x""; then
  echo "Setting hostendian flags = $sim_hostendian" 6>&1
fi],[
if test "x$cross_compiling" = "xno"; then
  AC_C_BIGENDIAN
  if test $ac_cv_c_bigendian = yes; then
    sim_hostendian="-DWITH_HOST_BYTE_ORDER=BIG_ENDIAN"
  else
    sim_hostendian="-DWITH_HOST_BYTE_ORDER=LITTLE_ENDIAN"
  fi
else
  sim_hostendian="-DWITH_HOST_BYTE_ORDER=0"
fi])dnl


AC_ARG_ENABLE(sim-icache,
[  --enable-sim-icache=size		Specify instruction-decode cache size and type.],
[icache="-R"
 case "${enableval}" in
  yes)		icache="1024"; sim_icache="-I $icache";;
  no)		sim_icache="-R";;
  *) icache=1024
     sim_icache="-"
     for x in `echo "${enableval}" | sed -e "s/,/ /g"`; do
       case "$x" in
         define)	sim_icache="${sim_icache}R";;
         semantic)	sim_icache="${sim_icache}C";;
	 insn)		sim_icache="${sim_icache}S";;
	 0*|1*|2*|3*|4*|5*|6*|7*|8*|9*)	icache=$x;;
         *)		AC_MSG_ERROR("Unknown value $x for --enable-sim-icache"); sim_icache="";;
       esac
     done
     sim_icache="${sim_icache}I $icache";;
esac
if test x"$silent" != x"yes" && test x"$icache" != x""; then
  echo "Setting instruction cache size to $icache ($sim_icache)"
fi],[sim_icache="-CSRI 1024"
if test x"$silent" != x"yes"; then
  echo "Setting instruction cache size to 1024 ($sim_icache)"
fi])dnl


AC_ARG_ENABLE(sim-inline,
[  --enable-sim-inline=inlines		Specify which functions should be inlined.],
[sim_inline=""
case "$enableval" in
  no)		sim_inline="-DDEFAULT_INLINE=0";;
  0)		sim_inline="-DDEFAULT_INLINE=0";;
  yes | 2)	sim_inline="-DDEFAULT_INLINE=ALL_INLINE";;
  1)		sim_inline="-DDEFAULT_INLINE=PSIM_INLINE_LOCALS";;
  *) for x in `echo "$enableval" | sed -e "s/,/ /g"`; do
       new_flag=""
       case "$x" in
	 *_INLINE=*)	new_flag="-D$x";;
	 *=*)		new_flag=`echo "$x" | sed -e "s/=/_INLINE=/" -e "s/^/-D/"`;;
	 *_INLINE)	new_flag="-D$x=ALL_INLINE";;
	 *)		new_flag="-D$x""_INLINE=ALL_INLINE";;
       esac
       if test x"$sim_inline" = x""; then
	 sim_inline="$new_flag"
       else
	 sim_inline="$sim_inline $new_flag"
       fi
     done;;
esac
if test x"$silent" != x"yes" && test x"$sim_inline" != x""; then
  echo "Setting inline flags = $sim_inline" 6>&1
fi],[if test x"$GCC" != ""; then
  sim_inline="-DDEFAULT_INLINE=PSIM_INLINE_LOCALS"
  if test x"$silent" != x"yes"; then
    echo "Setting inline flags = $sim_inline" 6>&1
  fi
else
  sim_inline=""
fi])dnl


AC_ARG_ENABLE(sim-jump,
[  --enable-sim-jump		Jump between semantic code (instead of call/return).],
[case "${enableval}" in
  yes)	sim_jump="-J";;
  no)	sim_jump="";;
  *)	AC_MSG_ERROR("--enable-sim-jump does not take a value"); sim_jump="";;
esac
if test x"$silent" != x"yes" && test x"$sim_jump" != x""; then
  echo "Setting jump flag = $sim_jump" 6>&1
fi],[sim_jump=""
if test x"$silent" != x"yes"; then
  echo "Setting jump flag = $sim_jump" 6>&1
fi])dnl


AC_ARG_ENABLE(sim-line-nr,
[  --enable-sim-line-nr=opts		Generate extra CPP code that references source rather than generated code],
[case "${enableval}" in
  yes)	sim_line_nr="";;
  no)	sim_line_nr="-L";;
  *)	AC_MSG_ERROR("--enable-sim-line-nr does not take a value"); sim_line_nr="";;
esac
if test x"$silent" != x"yes" && test x"$sim_line_nr" != x""; then
  echo "Setting warning flags = $sim_line_nr" 6>&1
fi],[sim_line_nr=""])dnl


AC_ARG_ENABLE(sim-model,
[  --enable-sim-model=which		Specify PowerPC to model.],
[case "${enableval}" in
  yes|no)	AC_MSG_ERROR("No value supplied for --enable-sim-model=model");;
  *)		sim_model="-DWITH_MODEL=${enableval}";;
esac
if test x"$silent" != x"yes" && test x"$sim_model" != x""; then
  echo "Setting model flags = $sim_model" 6>&1
fi],[sim_model=""])dnl


AC_ARG_ENABLE(sim-model-issue,
[  --enable-sim-model-issue		Specify whether to simulate model specific actions],
[case "${enableval}" in
  yes)	sim_model_issue="-DWITH_MODEL_ISSUE=MODEL_ISSUE_PROCESS";;
  no)	sim_model_issue="-DWITH_MODEL_ISSUE=MODEL_ISSUE_IGNORE";;
  *)	AC_MSG_ERROR("--enable-sim-model-issue does not take a value"); sim_model_issue="";;
esac
if test x"$silent" != x"yes"; then
  echo "Setting model-issue flags = $sim_model_issue" 6>&1
fi],[sim_model_issue=""])dnl


AC_ARG_ENABLE(sim-monitor,
[  --enable-sim-monitor=mon		Specify whether to enable monitoring events.],
[case "${enableval}" in
  yes)		sim_monitor="-DWITH_MON='MONITOR_INSTRUCTION_ISSUE | MONITOR_LOAD_STORE_UNIT'";;
  no)		sim_monitor="-DWITH_MON=0";;
  instruction)	sim_monitor="-DWITH_MON=MONITOR_INSTRUCTION_ISSUE";;
  memory)	sim_monitor="-DWITH_MON=MONITOR_LOAD_STORE_UNIT";;
  *)		AC_MSG_ERROR("Unknown value $enableval passed to --enable-sim-mon"); sim_env="";;
esac
if test x"$silent" != x"yes" && test x"$sim_monitor" != x""; then
  echo "Setting monitor flags = $sim_monitor" 6>&1
fi],[sim_monitor=""])dnl


AC_ARG_ENABLE(sim-opcode,
[  --enable-sim-opcode=which		Override default opcode lookup.],
[case "${enableval}" in
  yes|no)	AC_MSG_ERROR("No value supplied for --enable-sim-opcode=file");;
  *)		if test -f "${srcdir}/${enableval}"; then
		  sim_opcode="${enableval}"
		elif test -f "${srcdir}/dc-${enableval}"; then
		  sim_opcode="dc-${enableval}"
		else
		  AC_MSG_ERROR("File $enableval is not an opcode rules file");
		  sim_opcode="dc-complex"
		fi;;
esac
if test x"$silent" != x"yes" && test x"$sim_opcode" != x""; then
  echo "Setting opcode flags = $sim_opcode" 6>&1
fi],[sim_opcode="dc-complex"
if test x"$silent" != x"yes"; then
  echo "Setting opcode flags = $sim_opcode"
fi])dnl


AC_ARG_ENABLE(sim-packages,
[  --enable-sim-packages=list		Specify the packages to be included in the build.],
[packages=disklabel
case "${enableval}" in
  yes)	;;
  no)	AC_MSG_ERROR("List of packages must be specified for --enable-sim-packages"); packages="";;
  ,*)   packages="${packages}${enableval}";;
  *,)   packages="${enableval}${packages}";;
  *)	packages="${enableval}"'';;
esac
sim_pk_src=`echo $packages | sed -e 's/,/.c pk_/g' -e 's/^/pk_/' -e 's/$/.c/'`
sim_pk_obj=`echo $sim_pk_src | sed -e 's/\.c/.o/g'`
if test x"$silent" != x"yes" && test x"$packages" != x""; then
  echo "Setting packages to $sim_pk_src, $sim_pk_obj"
fi],[packages=disklabel
sim_pk_src=`echo $packages | sed -e 's/,/.c pk_/g' -e 's/^/pk_/' -e 's/$/.c/'`
sim_pk_obj=`echo $sim_pk_src | sed -e 's/\.c/.o/g'`
if test x"$silent" != x"yes"; then
  echo "Setting packages to $sim_pk_src, $sim_pk_obj"
fi])dnl


AC_ARG_ENABLE(sim-regparm,
[  --enable-sim-regparm=nr-parm		Pass parameters in registers instead of on the stack - x86/GCC specific.],
[case "${enableval}" in
  0*|1*|2*|3*|4*|5*|6*|7*|8*|9*) sim_regparm="-DWITH_REGPARM=${enableval}";;
  no)                            sim_regparm="" ;;
  yes)                           sim_regparm="-DWITH_REGPARM=3";;
  *)   AC_MSG_ERROR("Unknown value $enableval for --enable-sim-regparm"); sim_regparm="";;
esac
if test x"$silent" != x"yes" && test x"$sim_regparm" != x""; then
  echo "Setting regparm flags = $sim_regparm" 6>&1
fi],[sim_regparm=""])dnl


AC_ARG_ENABLE(sim-reserved-bits,
[  --enable-sim-reserved-bits		Specify whether to check reserved bits in instruction.],
[case "${enableval}" in
  yes)	sim_reserved="-DWITH_RESERVED_BITS=1";;
  no)	sim_reserved="-DWITH_RESERVED_BITS=0";;
  *)	AC_MSG_ERROR("--enable-sim-reserved-bits does not take a value"); sim_reserved="";;
esac
if test x"$silent" != x"yes" && test x"$sim_reserved" != x""; then
  echo "Setting reserved flags = $sim_reserved" 6>&1
fi],[sim_reserved=""])dnl


AC_ARG_ENABLE(sim-smp,
[  --enable-sim-smp=n			Specify number of processors to configure for.],
[case "${enableval}" in
  yes)	sim_smp="-DWITH_SMP=5" ; sim_igen_smp="-N 5";;
  no)	sim_smp="-DWITH_SMP=0" ; sim_igen_smp="-N 0";;
  *)	sim_smp="-DWITH_SMP=$enableval" ; sim_igen_smp="-N $enableval";;
esac
if test x"$silent" != x"yes" && test x"$sim_smp" != x""; then
  echo "Setting smp flags = $sim_smp" 6>&1
fi],[sim_smp="-DWITH_SMP=5" ; sim_igen_smp="-N 5"
if test x"$silent" != x"yes"; then
  echo "Setting smp flags = $sim_smp" 6>&1
fi])dnl


AC_ARG_ENABLE(sim-stdcall,
[  --enable-sim-stdcall=type		Use an alternative function call/return mechanism - x86/GCC specific.],
[case "${enableval}" in
  no)		sim_stdcall="" ;;
  std*)		sim_stdcall="-DWITH_STDCALL=1";;
  yes)		sim_stdcall="-DWITH_STDCALL=1";;
  *)   AC_MSG_ERROR("Unknown value $enableval for --enable-sim-stdcall"); sim_stdcall="";;
esac
if test x"$silent" != x"yes" && test x"$sim_stdcall" != x""; then
  echo "Setting function call flags = $sim_stdcall" 6>&1
fi],[sim_stdcall=""])dnl


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


AC_ARG_ENABLE(sim-switch,
[  --enable-sim-switch			Use a switch instead of a table for instruction call.],
[case "${enableval}" in
  yes)	sim_switch="-s";;
  no)	sim_switch="";;
  *)	AC_MSG_ERROR("--enable-sim-switch does not take a value"); sim_switch="";;
esac
if test x"$silent" != x"yes" && test x"$sim_switch" != x""; then
  echo "Setting switch flags = $sim_switch" 6>&1
fi],[sim_switch="";
if test x"$silent" != x"yes"; then
  echo "Setting switch flags = $sim_switch" 6>&1
fi])dnl


AC_ARG_ENABLE(sim-timebase,
[  --enable-sim-timebase			Specify whether the PPC timebase is supported.],
[case "${enableval}" in
  yes)	sim_timebase="-DWITH_TIME_BASE=1";;
  no)	sim_timebase="-DWITH_TIME_BASE=0";;
  *)	AC_MSG_ERROR("--enable-sim-timebase does not take a value"); sim_timebase="";;
esac
if test x"$silent" != x"yes" && test x"$sim_timebase" != x""; then
  echo "Setting timebase flags = $sim_timebase" 6>&1
fi],[sim_timebase=""])dnl


AC_ARG_ENABLE(sim-trace,
[  --enable-sim-trace			Specify whether tracing is supported.],
[case "${enableval}" in
  yes)	sim_trace="-DWITH_TRACE=1";;
  no)	sim_trace="-DWITH_TRACE=0";;
  *)	AC_MSG_ERROR("--enable-sim-trace does not take a value"); sim_trace="";;
esac
if test x"$silent" != x"yes" && test x"$sim_trace" != x""; then
  echo "Setting trace flags = $sim_trace" 6>&1
fi],[sim_trace=""])dnl


AC_ARG_ENABLE(sim-warnings,
[  --enable-sim-warnings=opts		Extra CFLAGS for turning on compiler warnings except for idecode.o, semantics.o and psim.o],
[case "${enableval}" in
  yes)	sim_warnings="-Werror -Wall -Wpointer-arith -Wmissing-prototypes -Wmissing-declarations ";;
  no)	sim_warnings="-w";;
  *)	sim_warnings=`echo "${enableval}" | sed -e "s/,/ /g"`;;
esac
if test x"$silent" != x"yes" && test x"$sim_warnings" != x""; then
  echo "Setting warning flags = $sim_warnings" 6>&1
fi],[sim_warnings=""])dnl


AC_ARG_ENABLE(sim-xor-endian,
[  --enable-sim-xor-endian=n		Specify number bytes involved in PowerPC XOR bi-endian mode (default 8).],
[case "${enableval}" in
  yes)	sim_xor_endian="-DWITH_XOR_ENDIAN=8";;
  no)	sim_xor_endian="-DWITH_XOR_ENDIAN=0";;
  *)	sim_xor_endian="-DWITH_XOR_ENDIAN=$enableval";;
esac
if test x"$silent" != x"yes" && test x"$sim_xor_endian" != x""; then
  echo "Setting xor-endian flag = $sim_xor_endian" 6>&1
fi],[sim_xor_endian=""])dnl


AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM

. ${srcdir}/../../bfd/configure.host

case ${host} in
  *mingw32*)
    AC_DEFINE(USE_WIN32API, 1,
              [Define if we should use the Windows API, instead of the 
	       POSIX API.  On Windows, we use the Windows API when 
	       building for MinGW, but the POSIX API when building 
	       for Cygwin.])
    ;;
esac	    

AC_CONFIG_HEADER(config.h:config.in)

AC_STRUCT_ST_BLKSIZE
AC_STRUCT_ST_BLOCKS
AC_STRUCT_ST_RDEV
AC_STRUCT_TIMEZONE

AC_TYPE_GETGROUPS
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T

AC_CHECK_FUNCS(access cfgetispeed cfgetospeed cfsetispeed cfsetospeed chdir chmod chown dup dup2 fchmod fchown fcntl fstat fstatfs getdirentries getegid geteuid getgid getpid getppid getrusage gettimeofday getuid ioctl kill link lseek lstat mkdir pipe readlink rmdir setreuid setregid stat sigprocmask stat symlink tcgetattr tcsetattr tcsendbreak tcdrain tcflush tcflow tcgetpgrp tcsetpgrp time umask unlink)

AC_CHECK_HEADERS(fcntl.h stdlib.h string.h strings.h sys/ioctl.h sys/mount.h sys/param.h sys/resource.h sys/stat.h sys/termio.h sys/termios.h sys/time.h sys/times.h sys/types.h time.h unistd.h sys/vfs.h sys/statfs.h)
AC_HEADER_DIRENT

dnl Figure out what type of termio/termios support there is
sim_termio=""
AC_MSG_CHECKING(for struct termios)
AC_CACHE_VAL(ac_cv_termios_struct,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/termios.h>],
[static struct termios x;
 x.c_iflag = 0;
 x.c_oflag = 0;
 x.c_cflag = 0;
 x.c_lflag = 0;
 x.c_cc[NCCS] = 0;],
ac_cv_termios_struct=yes, ac_cv_termios_struct=no)])
AC_MSG_RESULT($ac_cv_termios_struct)
if test $ac_cv_termios_struct = yes; then
  sim_termio="$sim_termio -DHAVE_TERMIOS_STRUCTURE"
fi

if test "$ac_cv_termios_struct" = "yes"; then
  AC_MSG_CHECKING(for c_line field in struct termios)
  AC_CACHE_VAL(ac_cv_termios_cline,
  [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/termios.h>],
[static struct termios x; x.c_line = 0;],
  ac_cv_termios_cline=yes, ac_cv_termios_cline=no)])

  AC_MSG_RESULT($ac_cv_termios_cline)
  if test $ac_cv_termios_cline = yes; then
    sim_termio="$sim_termio -DHAVE_TERMIOS_CLINE"
  fi
else
  ac_cv_termios_cline=no
fi

if test "$ac_cv_termios_struct" != "yes"; then
  AC_MSG_CHECKING(for struct termio)
  AC_CACHE_VAL(ac_cv_termio_struct,
  [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/termio.h>],
[static struct termio x;
 x.c_iflag = 0;
 x.c_oflag = 0;
 x.c_cflag = 0;
 x.c_lflag = 0;
 x.c_cc[NCC] = 0;],
ac_cv_termio_struct=yes, ac_cv_termio_struct=no)])
  AC_MSG_RESULT($ac_cv_termio_struct)
  if test $ac_cv_termio_struct = yes; then
    sim_termio="$sim_termio -DHAVE_TERMIO_STRUCTURE"
  fi
else
  ac_cv_termio_struct=no
fi

if test "$ac_cv_termio_struct" = "yes"; then
  AC_MSG_CHECKING(for c_line field in struct termio)
  AC_CACHE_VAL(ac_cv_termio_cline,
  [AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/termio.h>],
[static struct termio x; x.c_line = 0;],
  ac_cv_termio_cline=yes, ac_cv_termio_cline=no)])

  AC_MSG_RESULT($ac_cv_termio_cline)
  if test $ac_cv_termio_cline = yes; then
    sim_termio="$sim_termio -DHAVE_TERMIO_CLINE"
  fi
else
  ac_cv_termio_cline=no
fi

dnl Check for struct statfs
AC_MSG_CHECKING(for struct statfs)
AC_CACHE_VAL(ac_cv_struct_statfs,
[AC_TRY_COMPILE([#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif],
[static struct statfs s;],
ac_cv_struct_statfs=yes, ac_cv_struct_statfs=no)])
AC_MSG_RESULT($ac_cv_struct_statfs)
if test $ac_cv_struct_statfs = yes; then
  AC_DEFINE(HAVE_STRUCT_STATFS, 1,
	    [Define if struct statfs is defined in <sys/mount.h>])
fi

AC_CHECK_TYPES(long long)

dnl Figure out if /dev/zero exists or not
sim_devzero=""
AC_MSG_CHECKING(for /dev/zero)
AC_CACHE_VAL(ac_cv_devzero,
[AC_TRY_RUN([#include <fcntl.h>
main () {
  char buf[2048];
  int i;
  int fd = open ("/dev/zero", O_RDONLY);
  if (fd < 0)
    return 1;
  for (i = 0; i < sizeof (buf); i++)
    buf[i] = 1;
  if (read (fd, buf, sizeof (buf)) != sizeof (buf))
    return 1;
  for (i = 0; i < sizeof (buf); i++)
    if (buf[i])
      return 1;
  return 0;
}],[ac_cv_devzero=yes],[ac_cv_devzero=no],[ac_cv_devzero=no])])
AC_MSG_RESULT($ac_cv_devzero)
if test $ac_cv_devzero = yes; then
  sim_devzero="-DHAVE_DEVZERO"
else
  sim_devzero=""
fi

dnl Figure out if we are in the new Cygnus tree with a common directory or not
AC_MSG_CHECKING(for common simulator directory)
if test -f "${srcdir}/../common/callback.c"; then
  AC_MSG_RESULT(yes)
  sim_callback="callback.o targ-map.o"
  sim_targ_vals="targ-vals.h targ-map.c targ-vals.def"
else
  AC_MSG_RESULT(no)
  sim_callback=""
  sim_targ_vals=""
fi

AC_MSG_CHECKING(for common simulator directory fpu implementation)
if test -f "${srcdir}/../common/sim-fpu.c"; then
  AC_MSG_RESULT(yes)
  sim_fpu_cflags="-DHAVE_COMMON_FPU -I../common -I${srcdir}/../common"
  sim_fpu="sim-fpu.o"
else
  AC_MSG_RESULT(no)
  sim_fpu_cflags=
  sim_fpu=
fi

dnl Check for exe extension
AC_EXEEXT

AC_SUBST(CC_FOR_BUILD)
AC_SUBST(CFLAGS_FOR_BUILD)
AC_SUBST(CFLAGS)
AC_SUBST(HDEFINES)
AR=${AR-ar}
AC_SUBST(AR)
AC_PROG_RANLIB
AC_SUBST(sim_cflags)
AC_SUBST(sim_warnings)
AC_SUBST(sim_line_nr)
AC_SUBST(sim_config)
AC_SUBST(sim_opcode)
AC_SUBST(sim_switch)
AC_SUBST(sim_dup)
AC_SUBST(sim_decode_mechanism)
AC_SUBST(sim_jump)
AC_SUBST(sim_filter)
AC_SUBST(sim_icache)
AC_SUBST(sim_hw_src)
AC_SUBST(sim_hw_obj)
AC_SUBST(sim_pk_src)
AC_SUBST(sim_pk_obj)
AC_SUBST(sim_inline)
AC_SUBST(sim_bswap)
AC_SUBST(sim_endian)
AC_SUBST(sim_regparm)
AC_SUBST(sim_stdcall)
AC_SUBST(sim_xor_endian)
AC_SUBST(sim_hostendian)
AC_SUBST(sim_smp)
AC_SUBST(sim_igen_smp)
AC_SUBST(sim_bitsize)
AC_SUBST(sim_hostbitsize)
AC_SUBST(sim_env)
AC_SUBST(sim_timebase)
AC_SUBST(sim_alignment)
AC_SUBST(sim_float)
AC_SUBST(sim_trace)
AC_SUBST(sim_assert)
AC_SUBST(sim_reserved)
AC_SUBST(sim_monitor)
AC_SUBST(sim_model)
AC_SUBST(sim_default_model)
AC_SUBST(sim_model_issue)
AC_SUBST(sim_stdio)
AC_SUBST(sim_termio)
AC_SUBST(sim_devzero)
AC_SUBST(sim_callback)
AC_SUBST(sim_targ_vals)
AC_SUBST(sim_fpu_cflags)
AC_SUBST(sim_fpu)

AC_OUTPUT(Makefile,
[case x$CONFIG_HEADERS in xconfig.h:config.in) echo > stamp-h ;; esac])
