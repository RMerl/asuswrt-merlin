#!/bin/sh
DIR=.
if [ "x$1" != "x" ]
then
	DIR="$1"
fi

OLD=`pwd`
cd $DIR

echo "<section xmlns:xi=\"http://www.w3.org/2003/XInclude\">"
for I in `find . -mindepth 2 -type f -name '*.xml' | sort -t/ -k3 | xargs`
do 
	echo "<xi:include href='$I' parse='xml'/>"
done
                
echo "</section>"

cd $OLD
