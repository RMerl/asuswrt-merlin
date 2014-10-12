#!/bin/sh
#
# Blackbox test for net conf/registry roundtrips.
#
# Copyright (C) 2010 Gregor Beck <gbeck@sernet.de>
# Copyright (C) 2011 Michael Adam <obnox@samba.org>

if [ $# -lt 3 ]; then
cat <<EOF
Usage: test_net_registry_roundtrip.sh SCRIPTDIR SERVERCONFFILE CONFIGURATION
EOF
exit 1;
fi

SCRIPTDIR="$1"
SERVERCONFFILE="$2"
CONFIGURATION="$3"

NET="$VALGRIND ${NET:-$BINDIR/net} $CONFIGURATION"


if test "x${RPC}" = "xrpc" ; then
	NETREG="${NET} -U${USERNAME}%${PASSWORD} -I ${SERVER_IP} rpc registry"
else
	NETREG="${NET} registry"
fi

test x"$TEST_FUNCTIONS_SH" != x"INCLUDED" && {
incdir=`dirname $0`/../../../testprogs/blackbox
. $incdir/subunit.sh
}

failed=0

SED_INVALID_PARAMS="{
s/lock directory/;&/g
s/lock dir/;&/g
s/modules dir/;&/g
s/logging/;&/g
s/status/;&/g
s/logdir/;&/g
s/read prediction/;&/g
s/mkprofile/;&/g
s/valid chars/;&/g
s/timesync/;&/g
s/sambaconf/;&/g
s/logtype/;&/g
s/servername/;&/g
s/postscript/;&/g
}"

REGPATH="HKLM\Software\Samba"

conf_roundtrip_step() {
    echo "CMD: $*" >>$LOG
    $@ 2>>$LOG
    RC=$?
    echo "RC: $RC" >> $LOG
    test "x$RC" = "x0" || {
        echo "ERROR: $@ failed (RC=$RC)" | tee -a $LOG
    }
    return $RC
#    echo -n .
}

LOGDIR_PREFIX="conf_roundtrip"

conf_roundtrip()
{
    local DIR=$(mktemp -d ${PREFIX}/${LOGDIR_PREFIX}_XXXXXX)
    local LOG=$DIR/log

    echo conf_roundtrip $1 > $LOG

    sed -e "$SED_INVALID_PARAMS" $1 >$DIR/conf_in

    conf_roundtrip_step $NET conf drop
    test "x$?" = "x0" || {
        return 1
    }

    test -z "$($NET conf list)" 2>>$LOG
    if [ "$?" = "1" ]; then
	echo "ERROR: conf drop failed" | tee -a $LOG
	return 1
    fi

    conf_roundtrip_step $NET conf import $DIR/conf_in
    test "x$?" = "x0" || {
        return 1
    }

    conf_roundtrip_step $NET conf list > $DIR/conf_exp
    test "x$?" = "x0" || {
        return 1
    }

    grep "\[global\]" $DIR/conf_exp >/dev/null 2>>$LOG
    if [ "$?" = "1" ]; then
	echo "ERROR: conf import => conf export failed" | tee -a $LOG
	return 1
    fi

    conf_roundtrip_step $NET -d10 registry export $REGPATH $DIR/conf_exp.reg
    test "x$?" = "x0" || {
        return 1
    }

    conf_roundtrip_step $NET conf drop
    test "x$?" = "x0" || {
        return 1
    }

    test -z "$($NET conf list)" 2>>$LOG
    if [ "$?" = "1" ]; then
	echo "ERROR: conf drop failed" | tee -a $LOG
	return 1
    fi

    conf_roundtrip_step $NET registry import $DIR/conf_exp.reg
    test "x$?" = "x0" || {
        return 1
    }

    conf_roundtrip_step $NET conf list >$DIR/conf_out
    test "x$?" = "x0"  || {
        return 1
    }

    diff -q $DIR/conf_out $DIR/conf_exp  >> $LOG
    if [ "$?" = "1" ]; then
	echo "ERROR: registry import => conf export failed"  | tee -a $LOG
	return 1
    fi

    conf_roundtrip_step $NET registry export $REGPATH $DIR/conf_out.reg
    test "x$?" = "x0" || {
        return 1
    }

    diff -q $DIR/conf_out.reg $DIR/conf_exp.reg >>$LOG
    if [ "$?" = "1" ]; then
	echo "Error: registry import => registry export failed" | tee -a $LOG
	return 1
    fi
    rm -r $DIR
}

CONF_FILES=${CONF_FILES:-$(find $SRCDIR/ -name '*.conf' | grep -v examples/logon | xargs grep -l "\[global\]")}

# remove old logs:
for OLDDIR in $(find ${PREFIX} -type d -name "${LOGDIR_PREFIX}_*") ; do
	echo "removing old directory ${OLDDIR}"
	rm -rf ${OLDDIR}
done

for conf_file in $CONF_FILES
do
    testit "conf_roundtrip $conf_file" \
	conf_roundtrip $conf_file \
	|| failed=`expr $failed + 1`
done

testok $0 $failed

