#! /bin/sh
# Generate CGEN simulator files.
#
# Usage: /bin/sh cgen.sh {"arch"|"cpu"|"decode"|"defs"|"cpu-decode"} \
#	srcdir cgen cgendir cgenflags \
#	arch archflags cpu mach suffix archfile extrafiles opcfile
#
# We store the generated files in the source directory until we decide to
# ship a Scheme interpreter (or other implementation) with gdb/binutils.
# Maybe we never will.

# We want to behave like make, any error forces us to stop.
set -e

action=$1
srcdir=$2
cgen="$3"
cgendir=$4
cgenflags=$5
arch=$6
archflags=$7
cpu=$8
isa=$9
# portably bring parameters beyond $9 into view
shift ; mach=$9
shift ; suffix=$9
shift ; archfile=$9
shift ; extrafiles=$9
shift ; opcfile=$9

rootdir=${srcdir}/../..

test -z "${opcfile}" && opcfile=/dev/null

if test -z "$isa" ; then
  isa=all
  prefix=$cpu
else
  prefix=${cpu}_$isa
fi

lowercase='abcdefghijklmnopqrstuvwxyz'
uppercase='ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ARCH=`echo ${arch} | tr "${lowercase}" "${uppercase}"`
CPU=`echo ${cpu} | tr "${lowercase}" "${uppercase}"`
PREFIX=`echo ${prefix} | tr "${lowercase}" "${uppercase}"`

sedscript="\
-e s/@ARCH@/${ARCH}/g -e s/@arch@/${arch}/g \
-e s/@CPU@/${CPU}/g -e s/@cpu@/${cpu}/g \
-e s/@PREFIX@/${PREFIX}/g -e s/@prefix@/${prefix}/g"

case $action in
arch)
	rm -f tmp-arch.h1 tmp-arch.h
	rm -f tmp-arch.c1 tmp-arch.c
	rm -f tmp-all.h1 tmp-all.h

	${cgen} ${cgendir}/cgen-sim.scm \
		-s ${cgendir} \
		${cgenflags} \
		-f "${archflags}" \
		-m ${mach} \
		-a ${archfile} \
		-i ${isa} \
		-A tmp-arch.h1 \
		-B tmp-arch.c1 \
		-N tmp-all.h1
	sed $sedscript < tmp-arch.h1 > tmp-arch.h
	${rootdir}/move-if-change tmp-arch.h ${srcdir}/arch.h
	sed $sedscript < tmp-arch.c1 > tmp-arch.c
	${rootdir}/move-if-change tmp-arch.c ${srcdir}/arch.c
	sed $sedscript < tmp-all.h1 > tmp-all.h
	${rootdir}/move-if-change tmp-all.h ${srcdir}/cpuall.h

	rm -f tmp-arch.h1 tmp-arch.c1 tmp-all.h1
	;;

cpu | decode | cpu-decode)

	fileopts=""
	case $action in
	*cpu*)
		rm -f tmp-cpu.h1 tmp-cpu.c1
		rm -f tmp-ext.c1 tmp-read.c1 tmp-write.c1
		rm -f tmp-sem.c1 tmp-semsw.c1
		rm -f tmp-mod.c1
		rm -f tmp-cpu.h tmp-cpu.c
		rm -f tmp-ext.c tmp-read.c tmp-write.c
		rm -f tmp-sem.c tmp-semsw.c tmp-mod.c
		fileopts="$fileopts \
			-C tmp-cpu.h1 \
			-U tmp-cpu.c1 \
			-M tmp-mod.c1 \
			${extrafiles}"
		;;
	esac
	case $action in
	*decode*)
		rm -f tmp-dec.h1 tmp-dec.h tmp-dec.c1 tmp-dec.c
		fileopts="$fileopts \
			-T tmp-dec.h1 \
			-D tmp-dec.c1"
		case "$extrafiles" in
		  ignored) # Do nothing.
			   ;;
		  *)       fileopts="$fileopts $extrafiles"
			   ;;
		esac
		;;
	esac

	${cgen} ${cgendir}/cgen-sim.scm \
		-s ${cgendir} \
		${cgenflags} \
		-f "${archflags}" \
		-m ${mach} \
		-a ${archfile} \
		-i ${isa} \
		${fileopts}

	case $action in
	*cpu*)
		sed $sedscript < tmp-cpu.h1 > tmp-cpu.h
		${rootdir}/move-if-change tmp-cpu.h ${srcdir}/cpu${suffix}.h
		sed $sedscript < tmp-cpu.c1 > tmp-cpu.c
		${rootdir}/move-if-change tmp-cpu.c ${srcdir}/cpu${suffix}.c
		sed $sedscript < tmp-mod.c1 > tmp-mod.c
		${rootdir}/move-if-change tmp-mod.c ${srcdir}/model${suffix}.c
		if test -f tmp-ext.c1 ; then \
			sed $sedscript < tmp-ext.c1 > tmp-ext.c ; \
			${rootdir}/move-if-change tmp-ext.c ${srcdir}/extract${suffix}.c ; \
		fi
		if test -f tmp-read.c1 ; then \
			sed $sedscript < tmp-read.c1 > tmp-read.c ; \
			${rootdir}/move-if-change tmp-read.c ${srcdir}/read${suffix}.c ; \
		fi
		if test -f tmp-write.c1 ; then \
			sed $sedscript < tmp-write.c1 > tmp-write.c ; \
			${rootdir}/move-if-change tmp-write.c ${srcdir}/write${suffix}.c ; \
		fi
		if test -f tmp-sem.c1 ; then \
			sed $sedscript < tmp-sem.c1 > tmp-sem.c ; \
			${rootdir}/move-if-change tmp-sem.c ${srcdir}/sem${suffix}.c ; \
		fi
		if test -f tmp-semsw.c1 ; then \
			sed $sedscript < tmp-semsw.c1 > tmp-semsw.c ; \
			${rootdir}/move-if-change tmp-semsw.c ${srcdir}/sem${suffix}-switch.c ; \
		fi

		rm -f tmp-cpu.h1 tmp-cpu.c1
		rm -f tmp-ext.c1 tmp-read.c1 tmp-write.c1
		rm -f tmp-sem.c1 tmp-semsw.c1 tmp-mod.c1
		;;
	esac

	case $action in
	*decode*)
		sed $sedscript < tmp-dec.h1 > tmp-dec.h
		${rootdir}/move-if-change tmp-dec.h ${srcdir}/decode${suffix}.h
		sed $sedscript < tmp-dec.c1 > tmp-dec.c
		${rootdir}/move-if-change tmp-dec.c ${srcdir}/decode${suffix}.c

		if test -f tmp-sem.c1 ; then \
			sed $sedscript < tmp-sem.c1 > tmp-sem.c ; \
			${rootdir}/move-if-change tmp-sem.c ${srcdir}/sem${suffix}.c ; \
		fi
		if test -f tmp-semsw.c1 ; then \
			sed $sedscript < tmp-semsw.c1 > tmp-semsw.c ; \
			${rootdir}/move-if-change tmp-semsw.c ${srcdir}/sem${suffix}-switch.c ; \
		fi

		rm -f tmp-dec.h1 tmp-dec.c1
		;;
	esac

	;;

defs)
	rm -f tmp-defs.h1 tmp-defs.h
	
	${cgen} ${cgendir}/cgen-sim.scm \
		-s ${cgendir} \
		${cgenflags} \
		-f "${archflags}" \
		-m ${mach} \
		-a ${archfile} \
		-i ${isa} \
		-G tmp-defs.h1
	sed $sedscript < tmp-defs.h1 > tmp-defs.h
	${rootdir}/move-if-change tmp-defs.h ${srcdir}/defs${suffix}.h
	;;

desc)
	rm -f tmp-desc.h1 tmp-desc.h
	rm -f tmp-desc.c1 tmp-desc.c
	rm -f tmp-opc.h1 tmp-opc.h

	${cgen} ${cgendir}/cgen-opc.scm \
		-s ${cgendir} \
		${cgenflags} \
		-OPC ${opcfile} \
		-f "${archflags}" \
		-m ${mach} \
		-a ${archfile} \
		-i ${isa} \
		-H tmp-desc.h1 \
		-C tmp-desc.c1 \
		-O tmp-opc.h1
	sed $sedscript < tmp-desc.h1 > tmp-desc.h
	${rootdir}/move-if-change tmp-desc.h ${srcdir}/${arch}-desc.h
	sed $sedscript < tmp-desc.c1 > tmp-desc.c
	${rootdir}/move-if-change tmp-desc.c ${srcdir}/${arch}-desc.c
	sed $sedscript < tmp-opc.h1 > tmp-opc.h
	${rootdir}/move-if-change tmp-opc.h ${srcdir}/${arch}-opc.h

	rm -f tmp-desc.h1 tmp-desc.c1 tmp-opc.h1
	;;

*)
	echo "`basename $0`: unknown action: ${action}" >&2
	exit 1
	;;

esac

exit 0
