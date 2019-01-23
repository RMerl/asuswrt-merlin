#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: blackbox_group.sh PREFIX
EOF
exit 1;
fi

PREFIX="$1"
shift 1

. `dirname $0`/../../../testprogs/blackbox/subunit.sh


samba_tool="./bin/samba-tool"

CONFIG="--configfile=$PREFIX/etc/smb.conf"

#creation of two test subjects
testit "addspn" $samba_tool spn add FOO/bar Administrator $CONFIG
testit "delspn" $samba_tool spn delete FOO/bar $CONFIG
testit "readdspn" $samba_tool spn add FOO/bar Administrator $CONFIG
testit_expect_failure "failexistingspn" $samba_tool spn add FOO/bar Guest $CONFIG
testit "existingspnforce" $samba_tool spn add --force FOO/bar Guest  $CONFIG
testit_expect_failure "faildelspnnotgooduser" $samba_tool spn delete FOO/bar krbtgt $CONFIG
testit_expect_failure "faildelspnmoreoneuser" $samba_tool spn delete FOO/bar $CONFIG
testit "deluserspn" $samba_tool spn delete FOO/bar Guest $CONFIG
testit "dellastuserspn" $samba_tool spn delete FOO/bar $CONFIG
testit_expect_failure "faildelspn" $samba_tool spn delete FOO/bar $CONFIG
testit_expect_failure "failaddspn" $samba_tool spn add FOO/bar nonexistinguser $CONFIG

exit $failed
