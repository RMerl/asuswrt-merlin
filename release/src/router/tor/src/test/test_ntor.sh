#!/bin/sh
# Validate Tor's ntor implementation.

exitcode=0

"${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/ntor_ref.py" test-tor || exitcode=1
"${PYTHON:-python}" "${abs_top_srcdir:-.}/src/test/ntor_ref.py" self-test || exitcode=1

exit ${exitcode}
