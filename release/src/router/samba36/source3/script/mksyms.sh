#! /bin/sh

#
# mksyms.sh
#
# Extract symbols to export from C-header files.
# output in version-script format for linking shared libraries.
#
# This is the shell wrapper for the mksyms.awk core script.
#
# Copyright (C) 2008 Michael Adam <obnox@samba.org>
#

LANG=C; export LANG
LC_ALL=C; export LC_ALL
LC_COLLATE=C; export LC_COLLATE

if [ $# -lt 2 ]
then
  echo "Usage: $0 awk output_file header_files"
  exit 1
fi

awk="$1"
shift

symsfile="$1"
shift
symsfile_tmp="$symsfile.$$.tmp~"

proto_src="`echo $@ | tr ' ' '\n' | sort | uniq `"

echo creating $symsfile

mkdir -p `dirname $symsfile`

${awk} -f `dirname $0`/mksyms.awk $proto_src > $symsfile_tmp

if cmp -s $symsfile $symsfile_tmp 2>/dev/null
then
  echo "$symsfile unchanged"
  rm $symsfile_tmp
else
  mv $symsfile_tmp $symsfile
fi
