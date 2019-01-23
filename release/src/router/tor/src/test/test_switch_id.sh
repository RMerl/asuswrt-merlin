#!/bin/sh

if test "`id -u`" != '0'; then
    echo "This test only works when run as root. Skipping." >&2
    exit 77
fi

if test "`id -u nobody`" = ""; then
    echo "This test requires that your system have a 'nobody' user. Sorry." >&2
    exit 1
fi

if test "$OVERRIDE_GCDA_PERMISSIONS_HACK" = "yes"; then
  find src -type f -name '*gcda' -print0 | xargs -0 chmod 0666
fi

"${builddir:-.}/src/test/test-switch-id" nobody setuid          || exit 1
"${builddir:-.}/src/test/test-switch-id" nobody root-bind-low   || exit 1
"${builddir:-.}/src/test/test-switch-id" nobody setuid-strict   || exit 1
"${builddir:-.}/src/test/test-switch-id" nobody built-with-caps || exit 0
# ... Go beyond this point only if we were built with capability support.

"${builddir:-.}/src/test/test-switch-id" nobody have-caps       || exit 1
"${builddir:-.}/src/test/test-switch-id" nobody setuid-keepcaps || exit 1

if test "$OVERRIDE_GCDA_PERMISSIONS_HACK" = "yes"; then
    find src -type f -name '*gcda' -print0 | xargs -0 chmod 0644
fi

echo "All okay"

exit 0
