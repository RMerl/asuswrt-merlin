#!/bin/sh
#
# be sure to remove all other RT-XX-MIB.txt first
# 
# gen.sh [mib-name] [node-name]


export MIBS=ALL
export MIBDIRS=`pwd`
MODEL_NAME=`echo $1 | sed 's/-MIB.txt//g'`
MODULE_NAME=$2
TARGET_PATH=$MIBDIRS/../asus_mibs/sysdeps/$MODEL_NAME/asus-mib

echo $MODEL_NAME
echo $MODULE_NAME
echo $TARGET_PATH
mib2c -c mib2c.old-api.conf $1 $MODULE_NAME
cp $MODULE_NAME.c $TARGET_PATH
cp $MODULE_NAME.h $TARGET_PATH
echo "done. then edit rw func in $2.c, .h"
