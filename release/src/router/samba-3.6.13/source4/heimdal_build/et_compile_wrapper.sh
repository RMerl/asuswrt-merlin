#!/bin/sh
#

SELF="$0"
SELFDIR=`dirname "${SELF}"`

DESTDIR="$1"
CMD="$2"
FILE="$3"
SOURCE="$4"
shift 4

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

CURDIR="`pwd`"

cd "${DESTDIR}" && {
	# Remove older copies beforehand - MIT's compile_et uses odd permissions for these
	# files, which makes Heimdal's compile_et fail mysteriously when writing to them.
	rm -f `basename "${FILE}" .et`.c
	rm -f `basename "${FILE}" .et`.h
	"${CMD}" "${FILE}" >&2 || exit 1;
	cd "${CURDIR}"
	TMP="${SOURCE}.$$"
	mv "${SOURCE}" "${TMP}" && {
		echo "#include \"config.h\"" > "${SOURCE}" && {
			cat "${TMP}" >> "${SOURCE}"
		}
	}
	rm -f "${TMP}"
} || {
	echo "${SELF}:cannot cd into '${DESTDIR}'" >&2;
	exit 1;
}

exit 0;
