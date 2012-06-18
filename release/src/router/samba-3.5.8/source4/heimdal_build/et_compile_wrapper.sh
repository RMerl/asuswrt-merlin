#!/bin/sh
#

SELF=$0
SELFDIR=`dirname ${SELF}`

BUILDDIR=$1
DESTDIR=$2

CMD=$3
FILE=$4
SOURCE=$5
shift 5

test -z "${BUILDDIR}" && {
	echo "${SELF}:BUILDDIR: '${BUILDDIR}'" >&2;
	exit 1;
}

test -z "${DESTDIR}" && {
	echo "${SELF}:DESTDIR: '${DESTDIR}'" >&2;
	exit 1;
}

test -z "${CMD}" && {
	echo "${SELF}:CMD: '${CMD}'" >&2;
	exit 1;
}

test -z "${FILE}" && {
	echo "${SELF}:FILE: '${FILE}'" >&2;
	exit 1;
}

test -z "${SOURCE}" && {
	echo "${SELF}:SOURCE: '${SOURCE}'" >&2;
	exit 1;
}

CURDIR=`pwd`

cd ${BUILDDIR} && {
	ABS_BUILDDIR=`pwd`
	cd ${CURDIR}
} || {
	echo "${SELF}:cannot cd into '${BUILDDIR}'" >&2;
	exit 1;
}

cd ${DESTDIR} && {
	${ABS_BUILDDIR}/${CMD} ${FILE} >&2 || exit 1;
	cd ${CURDIR}
	TMP="${SOURCE}.$$"
	mv ${SOURCE} ${TMP} && {
		echo "#include \"config.h\"" > ${SOURCE} && {
			cat ${TMP} >> ${SOURCE}
		}
	}
	rm ${TMP}
} || {
	echo "${SELF}:cannot cd into '${BUILDDIR}'" >&2;
	exit 1;
}

exit 0;
