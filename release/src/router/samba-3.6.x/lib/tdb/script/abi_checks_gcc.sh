#!/bin/bash

make clean

mkdir -p abi/common
mkdir -p abi/tools
ABI_CHECKS="-aux-info abi/\$@.X"
make ABI_CHECK="$ABI_CHECKS" CC="/usr/bin/gcc"

for i in abi/*/*.X; do cat $i | grep 'tdb\.h'; done | sort | uniq | awk -F "extern " '{ print $2 }' | sort > abi/signatures
grep '^extern' include/tdb.h | grep -v '"C"' | sort | uniq | awk -F "extern " '{ print $2 }' >> abi/signatures

cat > abi/exports << EOF
{
    global:
EOF
#Functions
cat abi/signatures | grep "(" | awk -F '(' '{ print $1 }' | awk -F ' ' '{ print "           "$NF";" }' | tr -d '*' | sort >> abi/exports
#global vars
cat abi/signatures | grep -v "(" | awk -F ';' '{print $1 }' | awk -F ' ' '{ print "           "$NF";" }' | tr -d '*' | sort >> abi/exports
cat >> abi/exports << EOF

    local: *;
};
EOF

diff -u tdb.signatures abi/signatures
if [ "$?" != "0" ]; then
    echo "WARNING: Possible ABI Change!!"
fi

diff -u tdb.exports abi/exports
if [ "$?" != "0" ]; then
    echo "WARNING: Export file may be outdated!!"
fi
