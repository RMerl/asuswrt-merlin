#!/bin/sh
# Test backtrace functionality.

exitcode=0

"${builddir:-.}/src/test/test-bt-cl" backtraces || exit 77
"${builddir:-.}/src/test/test-bt-cl" assert | "${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/bt_test.py" || exitcode=1
"${builddir:-.}/src/test/test-bt-cl" crash  | "${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/bt_test.py" || exitcode=1

exit ${exitcode}
