#!/bin/sh

# compare the generated config.h from a waf build with existing samba
# build

OLD_CONFIG=$HOME/samba_old/source3/include/config.h
if test "x$1" != "x" ; then
	OLD_CONFIG=$1
fi

if test "x$DIFF" = "x" ; then
	DIFF="comm -23"
fi

grep "^.define" bin/default/source3/include/config.h | sort > waf-config.h
grep "^.define" $OLD_CONFIG | sort > old-config.h

$DIFF old-config.h waf-config.h

