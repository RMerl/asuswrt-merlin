#!/bin/sh
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1995 Patrick Powell, San Diego State University
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: TESTSUPPORT/dbserver.sh
# PURPOSE: fake database server program
# dbserver.sh,v 3.1 1996/12/28 21:40:46 papowell Exp
########################################################################## 
#
# Idea: the LPD will send the name of a database entry to the
#  server on stdin;  the server will send back the information
#  on stdout
# 
# We simply scan the database files for the information of form
#  #key:<entry>
#  *****  - strip off this and write <entry> to stdout
#
PATH=/bin:/usr/bin
#echo DBSERVER $$ $0 $* 1>&2
#echo DBSERVER $$ "pwd " `/bin/pwd`  1>&2
delay=0
for i in $*
do
	case $i in
		-delay*) delay=`echo $i |sed -e 's/-delay//'` ;;
		-error*) error=`echo $i |sed -e 's/-error//'` ;;
		-*) ;;
		*) file=$i ;;
	esac
done
# wait a minute to simulate the delay
#echo DBSERVER $$ delay $delay 1>&2
if test "$delay" -ne 0 ; then
	#echo DBSERVER $$ sleeping $delay 1>&2
	sleep $delay;
	#echo DBSERVER $$ awake 1>&2
fi;
#echo DBSERVER processing 1>&2
# exit with error status
if test -n "$error";
then
	exit $error;
fi;
# pump stdin to stderr
read printer
#echo DBSERVER $$ PRINTER ${printer} from "'$file'": 1>&2
#echo sed -n "s/^#${printer}://p" "$file" 1>&2
sed -n "s/^#${printer}://p" $file
#echo DBSERVER $$ DONE 1>&2
exit 0;
