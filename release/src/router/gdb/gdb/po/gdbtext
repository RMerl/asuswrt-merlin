#!/bin/sh -e

if test $# -lt 3
then
    echo "Usage: $0 <xgettext> <package>  <directory> ..." 1>&2
    exit 0
fi

xgettext=$1 ; shift
package=$1 ; shift

for d in "$@"
do
  __directories="$__directories --directory=$d"
done

for d in "$@"
do
  (
      cd $d
      find * \
	  -name '*-stub.c' -prune -o \
	  -name 'testsuite' -prune -o \
	  -name 'init.c' -prune -o \
	  -name '*.[hc]' -print
  )
done | ${xgettext} \
    --default-domain=${package} \
    --copyright-holder="Free Software Foundation, Inc." \
    --add-comments \
    --files-from=- \
    --force-po \
    --debug \
    --language=c \
    --keyword=_ \
    --keyword=N_ \
    ${__directories} \
    -o po/${package}.pot
