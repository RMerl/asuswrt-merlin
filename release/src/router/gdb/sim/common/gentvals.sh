#!/bin/sh
# Usage: gentvals.sh target type dir files pattern cpp

target=$1
type=$2
dir=$3
# FIXME: Would be nice to process #include's in these files.
files=$4
pattern=$5
cpp=$6

# FIXME: need trap to remove tmp files.

rm -f tmpvals.list tmpvals.uniq
for f in $files
do
	if test -f $dir/$f ; then
		grep "#define[ 	]$pattern" $dir/$f | sed -e "s/^.*#define[ 	]\($pattern\)[ 	]*\([^ 	][^ 	]*\).*$/\1/" >> tmpvals.list
	fi
done

sort <tmpvals.list | uniq >tmpvals.uniq

rm -f tmpvals.h
for f in $files
do
	if test -f $dir/$f ; then
		echo "#include <$f>" >>tmpvals.h
	fi
done

cat tmpvals.uniq |
while read sym
do
	echo "#ifdef $sym" >>tmpvals.h
	echo 'DEFVAL { "'$sym'", '$sym ' },' >>tmpvals.h
	echo "#endif" >>tmpvals.h
done

if test -z "$target"
then
	echo "#ifdef ${type}_defs"
else
	echo "#ifdef NL_TARGET_$target"
	echo "#ifdef ${type}_defs"
fi

for f in $files
do
	if test -f $dir/$f ; then
		echo "/* from $f */"
	fi
done

if test -z "$target"
then
	echo "/* begin $type target macros */"
else
	echo "/* begin $target $type target macros */"
fi

$cpp -I$dir tmpvals.h | grep DEFVAL | sed -e 's/DEFVAL//' -e 's/  / /'

if test -z "$target"
then
	echo "/* end $type target macros */"
	echo "#endif"
else
	echo "/* end $target $type target macros */"
	echo "#endif"
	echo "#endif"
fi

rm -f tmpvals.list tmpvals.uniq tmpvals.h
