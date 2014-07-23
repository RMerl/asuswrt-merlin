#! /bin/sh

CONFIGFILE="osver.h"
OS_VERSION=`uname -r`

[ -f ${CONFIGFILE} ] && rm -r ${CONFIGFILE}

echo "#define OS_VERSION \"$OS_VERSION\"" >> ${CONFIGFILE}
echo "" >> ${CONFIGFILE}

exit 0
