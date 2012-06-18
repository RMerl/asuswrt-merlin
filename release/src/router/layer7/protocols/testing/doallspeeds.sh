#!/bin/bash

# Print a complete report of speeds.

# Relies on output format of ./timeit.sh

if [ ! $1 ]; then
	userspace=1
	kernel=1
elif [ $1 == "userspace" ]; then
	userspace=1
elif [ $1 == "kernel" ]; then
	kernel=1
else
	echo huh?  Say \"userspace\", \"kernel\" or nothing \(which does both\).
	exit 1
fi

printf proto
if [ $userspace ]; then printf \\tuserspace; fi
if [ $kernel ]; then printf \\tkernel; fi
printf \\n

for f in ../*/*.pat; do 
	printf `basename $f .pat`

	if [ $userspace ]; then 
		gtime=`./timeit.sh $f userspace   real | grep Total | cut -d\  -f 2`
		printf \\t$gtime
	fi
	if [ $kernel ]; then 
		htime=`./timeit.sh $f kernel real | grep Total | cut -d\  -f 2`
		printf \\t$htime
	fi
	printf \\n
done
