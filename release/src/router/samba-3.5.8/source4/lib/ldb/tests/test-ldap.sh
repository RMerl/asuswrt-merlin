#!/bin/sh

PATH=/usr/local/sbin:/usr/sbin:/sbin:$PATH
export PATH
SCHEMA_NEEDED="core nis cosine inetorgperson openldap"

# setup needed schema files
for f in $SCHEMA_NEEDED; do
    if [ ! -r tests/schema/$f.schema ]; then
	mkdir -p tests/schema
	if [ -r /etc/ldap/schema/$f.schema ]; then
	    ln -s /etc/ldap/schema/$f.schema tests/schema/$f.schema
	    continue;
	fi
	if [ -r /etc/openldap/schema/$f.schema ]; then
	    ln -s /etc/openldap/schema/$f.schema tests/schema/$f.schema
	    continue;
	fi

	echo "SKIPPING TESTS: you need the following OpenLDAP schema files"
	for f in $SCHEMA_NEEDED; do
	    echo "  $f.schema"
	done
	exit 0
    fi
done

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

LDB_URL=`$LDBDIR/tests/ldapi_url.sh`
export LDB_URL

PATH=bin:$PATH
export PATH

LDB_SPECIALS=0
export LDB_SPECIALS

if $LDBDIR/tests/init_slapd.sh && 
   $LDBDIR/tests/start_slapd.sh &&
   $LDBDIR/tests/test-generic.sh; then
    echo "ldap tests passed";
    ret=0
else
    echo "ldap tests failed";
    ret=$?
fi

#$LDBDIR/tests/kill_slapd.sh

exit $ret
