#!/bin/sh
#
# Find files which include the xalloc.h header, but which still call
# the unwrapped calloc and malloc.
#

cd "$(git rev-parse --show-toplevel)" || {
  echo "error: failed to chdir to git root"
  exit 1
}

git grep -zl '#include "xalloc.h"' |
  xargs -0 grep -nwE '[^x](([cm]|re)alloc|strdup)\('

result=$?

if [ $result -eq 123 ]; then
	exit 0			# not found
elif [ $result -eq 0 ]; then
	exit 1			# found
fi

exit $result
