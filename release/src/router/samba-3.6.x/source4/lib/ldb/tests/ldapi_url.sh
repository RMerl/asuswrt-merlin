#!/bin/sh

# aargh, did LDAP ever have to expose this crap to users ...

BASE=`pwd`

TMPDIR=$BASE/tests/tmp

LDAPI_ESCAPE=`echo $TMPDIR/ldapi | sed 's|/|%2F|g'`

echo "ldapi://$LDAPI_ESCAPE"
