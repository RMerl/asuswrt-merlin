#!/bin/sh
# Test backtrace functionality.

exitcode=0

export ASAN_OPTIONS="handle_segv=0:allow_user_segv_handler=1"
"${builddir:-.}/src/test/test-bt-cl" backtraces || exit $?
"${builddir:-.}/src/test/test-bt-cl" assert 2>&1 | "${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/bt_test.py" || exitcode="$?"
"${builddir:-.}/src/test/test-bt-cl" crash  2>&1 | "${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/bt_test.py" || exitcode="$?"

"${builddir:-.}/src/test/test-bt-cl" none || exitcode="$?"

exit ${exitcode}
