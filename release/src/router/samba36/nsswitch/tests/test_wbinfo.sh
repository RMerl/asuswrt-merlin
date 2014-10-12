#!/bin/sh
# Blackbox test for wbinfo
if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_wbinfo.sh DOMAIN USERNAME PASSWORD TARGET
EOF
exit 1;
fi

DOMAIN=$1
USERNAME=$2
PASSWORD=$3
TARGET=$4
shift 4

failed=0
samba4bindir="$BUILDDIR/bin"
wbinfo="$VALGRIND $samba4bindir/wbinfo$EXEEXT"

. `dirname $0`/../../testprogs/blackbox/subunit.sh

testfail() {
	name="$1"
	shift
	cmdline="$*"
	echo "test: $name"
	$cmdline
	status=$?
        if [ x$status = x0 ]; then
                echo "failure: $name"
        else
                echo "success: $name"
        fi
        return $status
}

knownfail() {
        name="$1"
        shift
        cmdline="$*"
        echo "test: $name"
        $cmdline
        status=$?
        if [ x$status = x0 ]; then
                echo "failure: $name [unexpected success]"
				status=1
        else
                echo "knownfail: $name"
				status=0
        fi
        return $status
}

# List users
testit "wbinfo -u against $TARGET" $wbinfo -u || failed=`expr $failed + 1`
# List groups
testit "wbinfo -g against $TARGET" $wbinfo -g || failed=`expr $failed + 1`
# Convert netbios name to IP
# Does not work yet
knownfail "wbinfo -N against $TARGET" $wbinfo -N $NETBIOSNAME || failed=`expr $failed + 1`
# Convert IP to netbios name
# Does not work yet
knownfail "wbinfo -I against $TARGET" $wbinfo -I $SERVER_IP || failed=`expr $failed + 1`

# Convert name to SID
testit "wbinfo -n against $TARGET" $wbinfo -n "$DOMAIN/$USERNAME" || failed=`expr $failed + 1`
admin_sid=`$wbinfo -n "$DOMAIN/$USERNAME" | cut -d " " -f1`
echo "$DOMAIN/$USERNAME resolved to $admin_sid"

testit "wbinfo -s $admin_sid against $TARGET" $wbinfo -s $admin_sid || failed=`expr $failed + 1`
admin_name=`$wbinfo -s $admin_sid | cut -d " " -f1| tr a-z A-Z`
echo "$admin_sid resolved to $admin_name"

tested_name=`echo $DOMAIN/$USERNAME | tr a-z A-Z`

echo "test: wbinfo -s check for sane mapping"
if test x$admin_name != x$tested_name; then
	echo "$admin_name does not match $tested_name"
	echo "failure: wbinfo -s check for sane mapping"
	failed=`expr $failed + 1`
else
	echo "success: wbinfo -s check for sane mapping"
fi

testit "wbinfo -n on the returned name against $TARGET" $wbinfo -n $admin_name || failed=`expr $failed + 1`
test_sid=`$wbinfo -n $tested_name | cut -d " " -f1`

echo "test: wbinfo -n check for sane mapping"
if test x$admin_sid != x$test_sid; then
	echo "$admin_sid does not match $test_sid"
	echo "failure: wbinfo -n check for sane mapping"
	failed=`expr $failed + 1`
else
	echo "success: wbinfo -n check for sane mapping"
fi

testit "wbinfo -U against $TARGET" $wbinfo -U 30000 || failed=`expr $failed + 1`

echo "test: wbinfo -U check for sane mapping"
sid_for_30000=`$wbinfo -U 30000`
if test x$sid_for_30000 != "xS-1-22-1-30000"; then
	echo "uid 30000 mapped to $sid_for_30000, not S-1-22-1-30000"
	echo "failure: wbinfo -U check for sane mapping"
	failed=`expr $failed + 1`
else
	echo "success: wbinfo -U check for sane mapping"
fi

admin_uid=`$wbinfo -S $admin_sid`

testit "wbinfo -G against $TARGET" $wbinfo -G 30000 || failed=`expr $failed + 1`

echo "test: wbinfo -G check for sane mapping"
sid_for_30000=`$wbinfo -G 30000`
if test x$sid_for_30000 != "xS-1-22-2-30000"; then
        echo "gid 30000 mapped to $sid_for_30000, not S-1-22-2-30000"
	echo "failure: wbinfo -G check for sane mapping"
	failed=`expr $failed + 1`
else
	echo "success: wbinfo -G check for sane mapping"
fi

testit "wbinfo -S against $TARGET" $wbinfo -S "S-1-22-1-30000" || failed=`expr $failed + 1`

echo "test: wbinfo -S check for sane mapping"
uid_for_sid=`$wbinfo -S S-1-22-1-30000`
if test 0$uid_for_sid -ne 30000; then
	echo "S-1-22-1-30000 mapped to $uid_for_sid, not 30000"
	echo "failure: wbinfo -S check for sane mapping"
	failed=`expr $failed + 1`
else
	echo "success: wbinfo -S check for sane mapping"
fi

testfail "wbinfo -S against $TARGET using invalid SID" $wbinfo -S "S-1-22-2-30000" && failed=`expr $failed + 1`

testit "wbinfo -Y against $TARGET" $wbinfo -Y "S-1-22-2-30000" || failed=`expr $failed + 1`

echo "test: wbinfo -Y check for sane mapping"
gid_for_sid=`$wbinfo -Y S-1-22-2-30000`
if test 0$gid_for_sid -ne 30000; then
	echo "S-1-22-2-30000 mapped to $gid_for_sid, not 30000"
	echo "failure: wbinfo -Y check for sane mapping"
	failed=`expr $failed + 1`
else
	echo "success: wbinfo -Y check for sane mapping"
fi

testfail "wbinfo -Y against $TARGET using invalid SID" $wbinfo -Y "S-1-22-1-30000" && failed=`expr $failed + 1`

testit "wbinfo -t against $TARGET" $wbinfo -t || failed=`expr $failed + 1`

#didn't really work anyway
knownfail "wbinfo  --trusted-domains against $TARGET" $wbinfo --trusted-domains || failed=`expr $failed + 1`
knownfail "wbinfo --all-domains against $TARGET" $wbinfo --all-domains || failed=`expr $failed + 1`

testit "wbinfo --own-domain against $TARGET" $wbinfo --own-domain || failed=`expr $failed + 1`

echo "test: wbinfo --own-domain against $TARGET check output"
own_domain=`$wbinfo --own-domain`
if test x$own_domain = x$DOMAIN; then
	echo "success: wbinfo --own-domain against $TARGET check output"
else
	echo "Own domain reported as $own_domain instead of $DOMAIN"
	echo "failure: wbinfo --own-domain against $TARGET check output"
	failed=`expr $failed + 1`
fi

# this does not work
knownfail "wbinfo --sequence against $TARGET" $wbinfo --sequence

# this is stubbed out now
testit "wbinfo -D against $TARGET" $wbinfo -D $DOMAIN || failed=`expr $failed + 1`

testit "wbinfo -i against $TARGET" $wbinfo -i "$DOMAIN/$USERNAME" || failed=`expr $failed + 1`

testit "wbinfo --uid-info against $TARGET" $wbinfo --uid-info $admin_uid || failed=`expr $failed + 1`

echo "test: wbinfo --group-info against $TARGET"
rawgid=`$wbinfo --group-info "Domain admins" | sed 's/.*:\([0-9][0-9]*\):/\1/'`
if test x$? = x0; then
	echo "success: wbinfo --group-info against $TARGET"
else
	echo "failure: wbinfo --group-info against $TARGET"
	failed=`expr $failed + 1`
fi

gid=`echo $rawgid | sed 's/.*:\([0-9][0-9]*\):/\1/'`
testit "wbinfo --gid-info against $TARGET" $wbinfo --gid-info $gid || failed=`expr $failed + 1`

testit "wbinfo -r against $TARGET" $wbinfo -r "$DOMAIN/$USERNAME" || failed=`expr $failed + 1`

testit "wbinfo --user-domgroups against $TARGET" $wbinfo --user-domgroups $admin_sid || failed=`expr $failed + 1`

testit "wbinfo --user-sids against $TARGET" $wbinfo --user-sids $admin_sid || failed=`expr $failed + 1`

testit "wbinfo -a against $TARGET with domain creds" $wbinfo -a "$DOMAIN/$USERNAME"%"$PASSWORD" || failed=`expr $failed + 1`

testit "wbinfo --getdcname against $TARGET" $wbinfo --getdcname=$DOMAIN

testit "wbinfo -p against $TARGET" $wbinfo -p || failed=`expr $failed + 1`

testit "wbinfo -K against $TARGET with domain creds" $wbinfo -K "$DOMAIN/$USERNAME"%"$PASSWORD" || failed=`expr $failed + 1`

testit "wbinfo --separator against $TARGET" $wbinfo --separator || failed=`expr $failed + 1`

exit $failed
