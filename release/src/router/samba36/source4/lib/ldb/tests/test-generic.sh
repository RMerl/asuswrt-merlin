#!/bin/sh

if [ -z "$LDB_SPECIALS" ]; then
    LDB_SPECIALS=1
    export LDB_SPECIALS
fi

echo "LDB_URL: $LDB_URL"

echo "Adding base elements"
$VALGRIND ldbadd$EXEEXT $LDBDIR/tests/test.ldif || exit 1

echo "Adding again - should fail"
$VALGRIND ldbadd$EXEEXT $LDBDIR/tests/test.ldif 2> /dev/null && {
    echo "Should have failed to add again - gave $?"
    exit 1
}

echo "Modifying elements"
$VALGRIND ldbmodify$EXEEXT $LDBDIR/tests/test-modify.ldif || exit 1

echo "Showing modified record"
$VALGRIND ldbsearch$EXEEXT '(uid=uham)'  || exit 1

echo "Rename entry"
OLDDN="cn=Ursula Hampster,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST"
NEWDN="cn=Hampster Ursula,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST"
$VALGRIND ldbrename$EXEEXT "$OLDDN" "$NEWDN"  || exit 1

echo "Showing renamed record"
$VALGRIND ldbsearch$EXEEXT '(uid=uham)' || exit 1

echo "Starting ldbtest"
$VALGRIND ldbtest$EXEEXT --num-records 100 --num-searches 10  || exit 1

if [ $LDB_SPECIALS = 1 ]; then
 echo "Adding index"
 $VALGRIND ldbadd$EXEEXT $LDBDIR/tests/test-index.ldif  || exit 1
fi

echo "Adding bad attributes - should fail"
$VALGRIND ldbadd$EXEEXT $LDBDIR/tests/test-wrong_attributes.ldif && {
    echo "Should fhave failed - gave $?"
    exit 1
}

echo "Testing indexed search"
$VALGRIND ldbsearch$EXEEXT '(uid=uham)'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(&(objectclass=person)(objectclass=person)(objectclass=top))' || exit 1
$VALGRIND ldbsearch$EXEEXT '(&(uid=uham)(uid=uham))'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(|(uid=uham)(uid=uham))'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(|(uid=uham)(uid=uham)(objectclass=OpenLDAPperson))'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(&(uid=uham)(uid=uham)(!(objectclass=xxx)))'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(&(objectclass=person)(uid=uham)(!(uid=uhamxx)))' uid \* \+ dn  || exit 1
$VALGRIND ldbsearch$EXEEXT '(&(uid=uham)(uid=uha*)(title=*))' uid || exit 1

# note that the "((" is treated as an attribute not an expression
# this matches the openldap ldapsearch behaviour of looking for a '='
# to see if the first argument is an expression or not
$VALGRIND ldbsearch$EXEEXT '((' uid || exit 1
$VALGRIND ldbsearch$EXEEXT '(objectclass=)' uid || exit 1
$VALGRIND ldbsearch$EXEEXT -b 'cn=Hampster Ursula,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST' -s base "" sn || exit 1

echo "Test wildcard match"
$VALGRIND ldbadd$EXEEXT $LDBDIR/tests/test-wildcard.ldif  || exit 1
$VALGRIND ldbsearch$EXEEXT '(cn=test*multi)'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(cn=*test*multi*)'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(cn=*test_multi)'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(cn=test_multi*)'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(cn=test*multi*test*multi)'  || exit 1
$VALGRIND ldbsearch$EXEEXT '(cn=test*multi*test*multi*multi_*)' || exit 1

echo "Starting ldbtest indexed"
$VALGRIND ldbtest$EXEEXT --num-records 100 --num-searches 500  || exit 1

echo "Testing one level search"
count=`$VALGRIND ldbsearch$EXEEXT -b 'ou=Groups,o=University of Michigan,c=TEST' -s one 'objectclass=*' none |grep '^dn' | wc -l`
if [ $count != 3 ]; then
    echo returned $count records - expected 3
    exit 1
fi

echo "Testing binary file attribute value"
$VALGRIND ldbmodify$EXEEXT $LDBDIR/tests/photo.ldif || exit 1
count=`$VALGRIND ldbsearch$EXEEXT '(cn=Hampster Ursula)' jpegPhoto | grep '^dn' | wc -l`
if [ $count != 1 ]; then
    echo returned $count records - expected 1
    exit 1
fi

echo "*TODO* Testing UTF8 upper lower case searches !!"

echo "Testing compare"
count=`$VALGRIND ldbsearch$EXEEXT '(cn>=t)' cn | grep '^dn' | wc -l`
if [ $count != 2 ]; then
    echo returned $count records - expected 2
    echo "this fails on openLdap ..."
fi

count=`$VALGRIND ldbsearch$EXEEXT '(cn<=t)' cn | grep '^dn' | wc -l`
if [ $count != 13 ]; then
    echo returned $count records - expected 13
    echo "this fails on openLdap ..."
fi

checkcount() {
    count=$1
    scope=$2
    basedn=$3
    expression="$4"
    n=`$VALGRIND ldbsearch$EXEEXT -s "$scope" -b "$basedn" "$expression" | grep '^dn' | wc -l`
    if [ $n != $count ]; then
	echo "Got $n but expected $count for $expression"
	bin/ldbsearch "$expression"
	exit 1
    fi
    echo "OK: $count $expression"
}

checkcount 0 'base' '' '(uid=uham)'
checkcount 0 'one' '' '(uid=uham)'

checkcount 1 'base' 'cn=Hampster Ursula,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST' '(uid=uham)'
checkcount 1 'one' 'ou=Alumni Association,ou=People,o=University of Michigan,c=TEST' '(uid=uham)'
checkcount 1 'one' 'ou=People,o=University of Michigan,c=TEST' '(ou=ldb  test)'
