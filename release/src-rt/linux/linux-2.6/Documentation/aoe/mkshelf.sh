#! /bin/sh

if test "$#" != "2"; then
	echo "Usage: sh `basename $0` {dir} {shelfaddress}" 1>&2
	echo "       n_partitions=16 sh `basename $0` {dir} {shelfaddress}" 1>&2
	exit 1
fi
n_partitions=${n_partitions:-16}
dir=$1
shelf=$2
nslots=16
maxslot=`echo $nslots 1 - p | dc`
MAJOR=152

set -e

minor=`echo $nslots \* $shelf \* $n_partitions | bc`
endp=`echo $n_partitions - 1 | bc`
for slot in `seq 0 $maxslot`; do
	for part in `seq 0 $endp`; do
		name=e$shelf.$slot
		test "$part" != "0" && name=${name}p$part
		rm -f $dir/$name
		mknod -m 0660 $dir/$name b $MAJOR $minor

		minor=`expr $minor + 1`
	done
done
