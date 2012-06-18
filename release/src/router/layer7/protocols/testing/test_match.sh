#!/bin/bash

extract()
{
	if [ -r $1 ]; then
	# this can miss pseudo-valid files that have crap after the pattern
		cat $1 | grep -v ^$ | grep -v ^# | tail -1
	else
		echo Argument is not a readable file > /dev/stderr
		exit 1
	fi
}

if [ ! $1 ]; then
	echo Please specify a pattern or pattern file.
	exit 1
fi

if [ ! $2 ]; then
	echo
	echo Using the userspace pattern and library.
	echo You can change this by saying \"kernel\" as the second argument.
	echo
	matchprog=./test_speed-userspace # no, really
elif [ $2 == "kernel" ]; then
	echo Using the kernel pattern and library.
	matchprog=./match_kernel
elif [ $2 == "userspace" ]; then
	echo Using the userspace pattern and library.
	matchprog=./test_speed-userspace
else
	echo Didn\'t understand what you wanted. Using the userspace library.
	matchprog=./test_speed-userspace
fi

if [ $3 ]; then
	times=$3
else
	times=500
	echo
	echo Doing 500 repetitions of each test.
	echo You can change this by giving a number as the third argument.
	echo
fi

if [ -x ./randchars ] && [ -x $matchprog ] && [ -x ./randprintable ]; then
	true
else
	echo Can\'t find randchars, $matchprog or randprintable.  
	echo They should be in this directory.  Did you say \"make\"?
	exit 1
fi

printf "Out of $times completely random streams, this many match: "

pattern="`extract $1`"

for f in `seq $times`; do
	if [ $3 ]; then printf . > /dev/stderr; fi
	if [ $2 ] && [ $2 == "kernel" ]; then
		if ! ./randchars | $matchprog "$pattern"; then exit 1; fi
	else
		if ! ./randchars | $matchprog -f $1 -n 1 -v; then exit 1; fi
	fi
done | grep -iE '^match' -c

printf "Out of $times printable random streams, this many match:  " 

for f in `seq $times`; do
	if [ $3 ]; then printf . > /dev/stderr; fi
	if [ $2 ] && [ $2 == "kernel" ]; then
		if ! ./randprintable | $matchprog "$pattern"; then 
                	exit 1
		fi
	else
		if ! ./randprintable | $matchprog -v -n 1 -f $1; then 
			exit 1
		fi
	fi
done | grep -iE '^match' -c
