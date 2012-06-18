#!/bin/sh
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1995 Patrick Powell, San Diego State University
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: TESTSUPPORT/filter.sh
# PURPOSE: test filter for LPR software
# filter.sh,v 3.1 1996/12/28 21:40:46 papowell Exp
########################################################################## 
#	Filter Dummy Test
# 
PATH=/bin:/usr/bin
echo FILTER $$ $0 $* 1>&2
echo FILTER $$ $0 $*
set
#echo FILTER $$ "pwd " `/bin/pwd`  1>&2
printenv
delay=0
interval=0
for i in "$@"
do
	case $i in
		-delay*) delay=`echo $i |sed -e 's/-delay//'` ;;
		-interval*) interval=`echo $i |sed -e 's/-interval//'` ;;
		-error*) error=`echo $i |sed -e 's/-error//'` ;;
		-s* ) statusfile=`expr $i : '-s\(.*\)'` ;;
		-*) ;;
		*) file=$i ;;
	esac
done
if [ -f /tmp/filter.error ] ; then
. /tmp/filter.error
fi

if [ "$statusfile" != "" ] ; then
	exec 3>>$statusfile;
else
	exec 3>&2
fi
#if test -n "$file";
#	then echo $0 $* >>$file
#	else echo "--- NO Accounting File ---" 1>&3
#fi
# echo information into output
echo $0 $*
# wait a minute to simulate the delay
echo FILTER $$ delay $delay 1>&3
# pump stdin to stdout
cat
#/usr/bin/id 1>&3
if test "$delay" -ne 0 ; then
	echo FILTER $$ "sleeping $delay, interval $interval" `date` 1>&3
	if test "$interval" != "0" ; then
		elapsed=0;
		while [ $elapsed -lt $delay ] ; do
			sleep $interval;
			elapsed=`expr $interval '+' $elapsed`
			#echo FILTER $$ done $elapsed 1>&2
			echo FILTER $$ done $elapsed `date` 1>&3
		done
	else
		sleep $delay;
	fi
	echo FILTER $$ awake 1>&3
fi;
echo FILTER processing 1>&3
# exit with error status
if test -n "$error";
then
    echo FILTER DONE ERROR $error 1>&3
	exit $error;
fi;
echo FILTER DONE 1>&3
exit 0;
