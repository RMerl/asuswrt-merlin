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

testit "openldap-backend" $PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --ldap-backend-type=openldap --targetdir=$PREFIX/openldap-backend --ldap-dryrun-mode --slapd-path=/dev/null
testit "openldap-mmr-backend" $PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --ldap-backend-type=openldap --targetdir=$PREFIX/openldap-mmr-backend --ol-mmr-urls='ldap://localdc1:9000,ldap://localdc2:9000,ldap://localdc3:9000' --ldap-dryrun-mode --slapd-path=/dev/null
testit "fedora-ds-backend" $PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --ldap-backend-type=openldap --targetdir=$PREFIX/openldap-backend --ldap-dryrun-mode --slapd-path=/dev/null

reprovision() {
        $PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --ldap-backend-type=openldap --targetdir=$PREFIX/openldap-backend-reprovision --ldap-dryrun-mode --slapd-path=/dev/null
        $PYTHON ./setup/provision --domain=FOO --realm=foo.example.com --ldap-backend-type=openldap --targetdir=$PREFIX/openldap-backend-reprovision --ldap-dryrun-mode --slapd-path=/dev/null
}

testit "reprovision-backend" reprovision

exit $failed
