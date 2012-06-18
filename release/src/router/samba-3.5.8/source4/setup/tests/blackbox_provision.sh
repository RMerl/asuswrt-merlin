#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: blackbox_provision.sh PREFIX
EOF
exit 1;
fi

PREFIX="$1"
shift 1

. `dirname $0`/../../../testprogs/blackbox/subunit.sh

testit "simple-default" $PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --targetdir=$PREFIX/simple-default
testit "simple-dc" $PYTHON ./setup/provision --server-role="dc" --domain=FOO --realm=foo.example.com --domain-sid=S-1-5-21-4177067393-1453636373-93818738 --targetdir=$PREFIX/simple-dc
testit "simple-member" $PYTHON ./setup/provision --server-role="member" --domain=FOO --realm=foo.example.com --targetdir=$PREFIX/simple-member
testit "simple-standalone" $PYTHON ./setup/provision --server-role="standalone" --domain=FOO --realm=foo.example.com --targetdir=$PREFIX/simple-standalone
testit "blank-dc" $PYTHON ./setup/provision --server-role="dc" --domain=FOO --realm=foo.example.com --domain-sid=S-1-5-21-4177067393-1453636373-93818738 --targetdir=$PREFIX/blank-dc --blank
testit "partitions-only-dc" $PYTHON ./setup/provision --server-role="dc" --domain=FOO --realm=foo.example.com --domain-sid=S-1-5-21-4177067393-1453636373-93818738 --targetdir=$PREFIX/partitions-only-dc --partitions-only

reprovision() {
	$PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --targetdir="$PREFIX/reprovision"
	$PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --targetdir="$PREFIX/reprovision"
}

testit "reprovision" reprovision

exit $failed
