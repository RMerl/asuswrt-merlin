#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: blackbox_setpassword.sh PREFIX
EOF
exit 1;
fi

PREFIX="$1"
shift 1

. `dirname $0`/../../../testprogs/blackbox/subunit.sh

testit "simple-dc" $PYTHON ./setup/provision --server-role="dc" --domain=FOO --realm=foo.example.com --domain-sid=S-1-5-21-4177067393-1453636373-93818738 --targetdir=$PREFIX/simple-dc

testit "newuser" $PYTHON ./setup/newuser --configfile=$PREFIX/simple-dc/etc/smb.conf testuser testpass

testit "setpassword" $PYTHON ./setup/setpassword --configfile=$PREFIX/simple-dc/etc/smb.conf testuser --newpassword=testpass

testit "setpassword" $PYTHON ./setup/setpassword --configfile=$PREFIX/simple-dc/etc/smb.conf testuser --newpassword=testpass --must-change-at-next-login

testit "pwsettings" $PYTHON ./setup/pwsettings --quiet set --configfile=$PREFIX/simple-dc/etc/smb.conf --complexity=default --history-length=default --min-pwd-length=default --min-pwd-age=default --max-pwd-age=default

exit $failed
