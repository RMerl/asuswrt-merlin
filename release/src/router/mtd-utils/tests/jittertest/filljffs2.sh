#!/bin/bash

# Pass following cmd line:
# 1st - file to copy
# 2nd - file to copy to
# 3rd - time to sleep between copies

while [ $(( 1 )) -gt $(( 0 )) ]
do
   cp $1 $2
   rm $2
   df |grep mtd > /dev/console
   echo "sleeping $3"
   sleep $3
done

