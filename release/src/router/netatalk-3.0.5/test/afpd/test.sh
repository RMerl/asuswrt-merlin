#!/bin/sh
if [ ! -d /tmp/AFPtestvolume ] ; then
    mkdir -p /tmp/AFPtestvolume
    if [ $? -ne 0 ] ; then
        echo Error creating AFP test volume /tmp/AFPtestvolume
        exit 1
    fi
fi

if [ ! -f test.conf ] ; then
    echo -n "Creating configuration template ... "
    cat > test.conf <<EOF
[Global]
afp port = 10548

[test]
path = /tmp/AFPtestvolume
cnid scheme = last
EOF
    echo [ok]
fi
