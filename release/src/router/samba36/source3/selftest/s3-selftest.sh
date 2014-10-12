#!/bin/sh

FILTER_XFAIL="${PYTHON} -u ${SELFTESTDIR}/filter-subunit --expected-failures=${SOURCEDIR}/selftest/knownfail"
if [ "x${SUBUNIT_FORMATTER}" = x"" ]; then
	SUBUNIT_FORMATTER="${PYTHON} -u ${SELFTESTDIR}/format-subunit --prefix=${SELFTESTPREFIX} --immediate"
fi

cleanup_and_exit() {
	if test "$1" = 0 -o -z "$1"; then
		exit 0
	else
		exit $1
	fi
}

st_test_done() {
	test -f ${SELFTESTPREFIX}/st_done || { echo "SELFTEST FAILED"; cleanup_and_exit 1; }
}

if [ "x${RUN_FROM_BUILD_FARM}" = "xyes" ]; then
	( rm -f ${SELFTESTPREFIX}/st_done && \
		${PERL} ${SELFTESTDIR}/selftest.pl \
			--builddir=. --prefix=${SELFTESTPREFIX} --target=samba3 \
			--testlist="${PYTHON} ${SOURCEDIR}/selftest/tests.py|" \
			--exclude=${SOURCEDIR}/selftest/skip \
	                --srcdir="${SOURCEDIR}/.." \
			--socket-wrapper ${TESTS} \
	&& touch ${SELFTESTPREFIX}/st_done ) | \
		${FILTER_XFAIL} --strip-passed-output
	EXIT_STATUS=$?

	st_test_done
else
	( rm -f ${SELFTESTPREFIX}/st_done && \
		${PERL} ${SELFTESTDIR}/selftest.pl \
			--builddir=. --prefix=${SELFTESTPREFIX} --target=samba3 \
			--testlist="${PYTHON} ${SOURCEDIR}/selftest/tests.py|" \
			--exclude=${SOURCEDIR}/selftest/skip \
	                --srcdir="${SOURCEDIR}/.." \
			--socket-wrapper ${TESTS} \
	&& touch ${SELFTESTPREFIX}/st_done ) | \
		${FILTER_XFAIL} | ${SUBUNIT_FORMATTER}
	EXIT_STATUS=$?

	st_test_done
fi

cleanup_and_exit ${EXIT_STATUS}
